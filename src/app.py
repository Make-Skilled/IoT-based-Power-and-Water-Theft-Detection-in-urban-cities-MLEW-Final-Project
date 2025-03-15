from web3 import Web3,HTTPProvider
from flask import Flask,render_template,redirect,request,session,url_for,jsonify
import json
from werkzeug.utils import secure_filename
import os
import hashlib
import requests
from datetime import datetime
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

app=Flask(__name__)
app.secret_key="M@keskilled0"

userManagementArtifactPath="./build/contracts/userManagement.json"
blockchainServer="http://127.0.0.1:7545"

# Add these email configuration constants at the top of your file
SMTP_SERVER = "smtp.gmail.com"
SMTP_PORT = 587
SMTP_USERNAME = "kr4785543@gmail.com"  # Replace with your email
SMTP_PASSWORD = "qhuzwfrdagfyqemk"      # Replace with your app password

def send_email(to_email, subject, body):
    try:
        # Create message
        msg = MIMEMultipart()
        msg['From'] = SMTP_USERNAME
        msg['To'] = to_email
        msg['Subject'] = subject
        
        # Add body
        msg.attach(MIMEText(body, 'html'))
        
        # Create SMTP session
        server = smtplib.SMTP(SMTP_SERVER, SMTP_PORT)
        server.starttls()
        server.login(SMTP_USERNAME, SMTP_PASSWORD)
        
        # Send email
        server.send_message(msg)
        server.quit()
        print(f"Email sent successfully to {to_email}")
        return True
    except Exception as e:
        print(f"Failed to send email: {str(e)}")
        return False

def connectWithContract(wallet, artifact=userManagementArtifactPath):
    try:
        web3 = Web3(HTTPProvider(blockchainServer))
        if not web3.isConnected():
            raise Exception("Failed to connect to blockchain server")
        print('Connected with Blockchain Server')

        # Set the default account
        if wallet == 0:
            web3.eth.defaultAccount = web3.eth.accounts[0]
        else:
            # Ensure the wallet address is checksum
            wallet = Web3.toChecksumAddress(wallet)
            web3.eth.defaultAccount = wallet
        print('Wallet Selected:', web3.eth.defaultAccount)

        # Load contract
        with open(artifact) as f:
            artifact_json = json.load(f)
            contract_abi = artifact_json['abi']
            try:
                contract_address = artifact_json['networks']['5777']['address']
            except KeyError:
                raise Exception("Contract not deployed to network")

        contract = web3.eth.contract(
            address=contract_address,
            abi=contract_abi
        )
        print('Contract Selected at:', contract_address)
        return contract, web3

    except FileNotFoundError:
        raise Exception("Contract artifact not found. Please ensure the contract is compiled and deployed")
    except json.JSONDecodeError:
        raise Exception("Invalid contract artifact format")
    except Exception as e:
        raise Exception(f"Failed to connect to contract: {str(e)}")

@app.route('/')
def homePage():
    return render_template('index.html')

@app.route('/login')
def loginPage():
    return render_template('login.html')

@app.route('/signup')
def signupPage():
    return render_template('signup.html')

@app.route('/dashboard')
def DashboardPage():
    return render_template('dashboard.html')

@app.route('/register',methods=['POST'])
def register():
    try:
        # Get form data with validation
        wallet = request.form.get('address', '').strip()
        username = request.form.get('username', '').strip()
        email = request.form.get('email', '').strip()
        password = request.form.get('password', '')
        confirmPassword = request.form.get('confirmPassword', '')

        # Input validation with specific error messages
        if not wallet:
            return render_template('signup.html', message='Wallet address is required')
        if not username:
            return render_template('signup.html', message='Username is required')
        if not email:
            return render_template('signup.html', message='Email is required')
        if not password:
            return render_template('signup.html', message='Password is required')
            
        if password != confirmPassword:
            return render_template('signup.html', message='Passwords do not match')
        
        # Validate wallet address format
        try:
            if not Web3.isAddress(wallet):
                return render_template('signup.html', message='Invalid wallet address format')
            wallet = Web3.toChecksumAddress(wallet)
        except Exception as e:
            print(f"Wallet validation error: {str(e)}")
            return render_template('signup.html', message='Invalid wallet address format')

        # Connect to contract
        try:
            contract, web3 = connectWithContract(wallet)
        except Exception as e:
            print(f"Contract connection error: {str(e)}")
            return render_template('signup.html', message=f'Blockchain connection error: {str(e)}')

        # Check if wallet is connected
        if not web3.isConnected():
            return render_template('signup.html', message='Unable to connect to blockchain network')

        # Hash the password
        hashed_password = hashlib.sha256(password.encode()).hexdigest()
        
        print(f"Attempting to register user:")
        print(f"Wallet: {wallet}")
        print(f"Username: {username}")
        print(f"Email: {email}")
        
        # Attempt to create the account
        try:
            tx_hash = contract.functions.userSignUp(
                wallet,
                username,
                hashed_password,
                email
            ).transact({
                'from': wallet,
                'gas': 3000000  # Specify gas limit
            })
            
            # Wait for transaction receipt
            receipt = web3.eth.wait_for_transaction_receipt(tx_hash, timeout=120)
            
            if receipt.status == 1:
                # Send welcome email
                email_subject = "Welcome to Theft Detection System"
                email_body = f"""
                <html>
                    <body>
                        <h2>Welcome to Theft Detection System!</h2>
                        <p>Hello {username},</p>
                        <p>Thank you for creating an account. Your registration was successful!</p>
                        <p><strong>Account Details:</strong></p>
                        <ul>
                            <li>Username: {username}</li>
                            <li>Wallet Address: {wallet}</li>
                            <li>Email: {email}</li>
                        </ul>
                        <p>You can now login to your account and start monitoring your resources.</p>
                        <p>If you have any questions, please don't hesitate to contact our support team.</p>
                    </body>
                </html>
                """
                send_email(email, email_subject, email_body)
                
                print('Transaction Successful')
                print(f'Transaction Hash: {tx_hash.hex()}')
                return render_template('login.html', message='Signup Successful! Please check your email and login.')
            else:
                print('Transaction Failed')
                print(f'Receipt: {receipt}')
                return render_template('signup.html', message='Transaction failed. Please try again.')
                
        except ValueError as ve:
            error_message = str(ve)
            print(f"Contract call error: {error_message}")
            
            if "already exists" in error_message.lower():
                return render_template('signup.html', message='Wallet address or username already registered')
            elif "revert" in error_message.lower():
                # Extract the revert reason if available
                revert_reason = error_message.split("revert: ")[-1] if "revert: " in error_message else "Transaction reverted"
                return render_template('signup.html', message=f'Contract error: {revert_reason}')
            else:
                return render_template('signup.html', message='Invalid input data. Please check your details.')
                
        except Exception as e:
            print(f"Transaction error: {str(e)}")
            return render_template('signup.html', message=f'Transaction error: {str(e)}')
            
    except Exception as e:
        print(f"Registration error: {str(e)}")
        return render_template('signup.html', message='An error occurred during registration')

@app.route('/loginForm', methods=['POST'])
def loginForm():
    try:
        username = request.form['username']
        password = request.form['password']

        # Hash the password using the same method as registration
        hashed_password = hashlib.sha256(password.encode()).hexdigest()

        contract, web3 = connectWithContract(0)
        
        try:
            # Pass the hashed password to the contract
            result = contract.functions.userLogin(username, hashed_password).call()
            
            if result:
                # Fetch user details
                response = contract.functions.viewUserByUsername(username).call()
                
                # Store user details in session
                session['userwallet'] = response[0]
                session['username'] = response[1]
                session['useremail'] = response[3]
                
                # Send login notification email
                email_subject = "New Login Detected - Theft Detection System"
                email_body = f"""
                <html>
                    <body>
                        <h2>New Login Detected</h2>
                        <p>Hello {session['username']},</p>
                        <p>We detected a new login to your account.</p>
                        <p><strong>Details:</strong></p>
                        <ul>
                            <li>Username: {session['username']}</li>
                            <li>Wallet Address: {session['userwallet']}</li>
                            <li>Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</li>
                        </ul>
                        <p>If this wasn't you, please contact support immediately.</p>
                    </body>
                </html>
                """
                send_email(session['useremail'], email_subject, email_body)
                
                return redirect(url_for('DashboardPage'))
            else:
                return render_template('login.html', message='Invalid credentials')
                
        except Exception as e:
            print(f"Login error: {str(e)}")
            return render_template('login.html', message='Invalid username or password')
            
    except Exception as e:
        print(f"Login form error: {str(e)}")
        return render_template('login.html', message='An error occurred during login')

@app.route('/logout')
def logout():
    session.clear()
    return redirect('/')



if __name__=="__main__":
    app.run(host='0.0.0.0',port=4001,debug=True)