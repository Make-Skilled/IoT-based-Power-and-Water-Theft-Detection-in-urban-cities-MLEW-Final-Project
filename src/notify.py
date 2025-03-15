import requests
import time
from datetime import datetime
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from web3 import Web3, HTTPProvider
import json

AUTHORITY_EMAIL="sudheerthadikonda0605@gmail.com"
AUTHORITY_ADDRESS="0x4ff1140b15A89085c81a063f3DA6E915100Eba13"

# Configuration
THINGSPEAK_CHANNEL_ID = "2853641"
THINGSPEAK_API_URL = f"https://api.thingspeak.com/channels/{THINGSPEAK_CHANNEL_ID}/feed.json"
CHECK_INTERVAL = 15  # seconds

# Email Configuration
SMTP_SERVER = "smtp.gmail.com"
SMTP_PORT = 587
SMTP_USERNAME = "kr4785543@gmail.com"
SMTP_PASSWORD = "qhuzwfrdagfyqemk"

# Blockchain Configuration
BLOCKCHAIN_SERVER = "http://127.0.0.1:7545"
USER_MANAGEMENT_ARTIFACT_PATH = "./build/contracts/userManagement.json"

def send_email(to_email, subject, body):
    try:
        msg = MIMEMultipart()
        msg['From'] = SMTP_USERNAME
        msg['To'] = to_email
        msg['Subject'] = subject
        msg.attach(MIMEText(body, 'html'))
        
        server = smtplib.SMTP(SMTP_SERVER, SMTP_PORT)
        server.starttls()
        server.login(SMTP_USERNAME, SMTP_PASSWORD)
        server.send_message(msg)
        server.quit()
        
        print(f"✅ Email sent successfully to {to_email}")
        return True
    except Exception as e:
        print(f"❌ Failed to send email: {str(e)}")
        return False

def connect_blockchain(wallet=0):
    """Connect to blockchain using the same configuration as app.py"""
    try:
        web3 = Web3(HTTPProvider(BLOCKCHAIN_SERVER))
        if not web3.isConnected():  # Changed from is_connected() to isConnected()
            raise Exception("Failed to connect to blockchain server")
        
        print('Connected with Blockchain Server')

        # Set the default account
        if wallet == 0:
            web3.eth.default_account = web3.eth.accounts[0]
        else:
            web3.eth.default_account = wallet

        # Load contract ABI and address from artifact
        with open(USER_MANAGEMENT_ARTIFACT_PATH, 'r') as f:
            artifact = json.load(f)
            
        contract_address = artifact['networks']['5777']['address']
        contract_abi = artifact['abi']
        
        contract = web3.eth.contract(address=contract_address, abi=contract_abi)
        return contract, web3
    except Exception as e:
        print(f"❌ Blockchain connection error: {str(e)}")
        return None, None

def send_blockchain_notification(contract, web3, user_wallet, alert_type, message, parameter, value):
    """Send notification using the contract's addNotification function"""
    try:
        # Convert user_wallet to checksum address
        user_wallet = Web3.toChecksumAddress(user_wallet)
        
        tx_hash = contract.functions.addNotification(
            user_wallet,  # address type
            str(alert_type),  # string type
            str(message),  # string type
            str(parameter),  # string type
            str(value)  # string type
        ).transact({
            'from': web3.eth.default_account,
            'gas': 3000000
        })
        receipt = web3.eth.wait_for_transaction_receipt(tx_hash)
        print(f"✅ Blockchain notification sent for {user_wallet}")
        return True
    except Exception as e:
        print(f"❌ Blockchain notification error: {str(e)}")
        return False

def check_thingspeak():
    try:
        response = requests.get(THINGSPEAK_API_URL)
        if response.status_code != 200:
            raise Exception(f"ThingSpeak API error: {response.status_code}")

        data = response.json()
        if not data['feeds'] or len(data['feeds']) == 0:
            print("No data available from ThingSpeak")
            return

        last_record = data['feeds'][-1]
        print(last_record)
        
        # Extract values
        power_theft = last_record.get('field4') == None
        water_theft = last_record.get('field6') == None
        
        if power_theft or water_theft:
            contract, web3 = connect_blockchain()
            if not contract or not web3:
                return
            
            # Use a single wallet address instead of getting all users
            wallet = AUTHORITY_ADDRESS  # Replace with your wallet address
            
            if power_theft:
                power_alert = {
                    'type': 'POWER',
                    'message': 'Power theft detected!',
                    'parameter': 'Power Consumption',
                    'value': f"{last_record.get('field2')} W"
                }
                                
                # Send email
                email_subject = "⚠️ Power Theft Alert - Theft Detection System"
                email_body = f"""
                <html>
                    <body>
                        <h2>⚠️ Power Theft Alert</h2>
                        <p>A power theft has been detected in your system.</p>
                        <p><strong>Details:</strong></p>
                        <ul>
                            <li>Current Power: {last_record.get('field2')} W</li>
                            <li>Current: {last_record.get('field1')} A</li>
                            <li>Energy: {last_record.get('field3')} kWh</li>
                            <li>Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</li>
                        </ul>
                        <p>Please check your system immediately.</p>
                    </body>
                </html>
                """
                send_email(AUTHORITY_EMAIL, email_subject, email_body)
                
                # Send blockchain notification
                send_blockchain_notification(
                    contract, web3, wallet,
                    power_alert['type'],
                    power_alert['message'],
                    power_alert['parameter'],
                    power_alert['value']
                )
            
            if water_theft:
                water_alert = {
                    'type': 'WATER',
                    'message': 'Water theft detected!',
                    'parameter': 'Flow Difference',
                    'value': f"Main: {last_record.get('field4')} L/min, User: {last_record.get('field5')} L/min"
                }
                
                # Send email
                email_subject = "⚠️ Water Theft Alert - Theft Detection System"
                email_body = f"""
                <html>
                    <body>
                        <h2>⚠️ Water Theft Alert</h2>
                        <p>A water theft has been detected in your system.</p>
                        <p><strong>Details:</strong></p>
                        <ul>
                            <li>Main Flow: {last_record.get('field4')} L/min</li>
                            <li>User Flow: {last_record.get('field5')} L/min</li>
                            <li>Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</li>
                        </ul>
                        <p>Please check your system immediately.</p>
                    </body>
                </html>
                """
                send_email(AUTHORITY_EMAIL, email_subject, email_body)
                
                # Send blockchain notification
                send_blockchain_notification(
                    contract, web3, wallet,
                    water_alert['type'],
                    water_alert['message'],
                    water_alert['parameter'],
                    water_alert['value']
                )

    except requests.RequestException as e:
        print(f"❌ API Request error: {str(e)}")
    except Exception as e:
        print(f"❌ Error in check_thingspeak: {str(e)}")

def main():
    print("Starting ThingSpeak monitoring service...")
    
    while True:
        print(f"\n⏰ Checking ThingSpeak at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        check_thingspeak()
        time.sleep(CHECK_INTERVAL)

if __name__ == "__main__":
    main()