from web3 import Web3,HTTPProvider
from flask import Flask,render_template,redirect,request,session,url_for
import json
from werkzeug.utils import secure_filename
import os
import hashlib

app=Flask(__name__)
app.secret_key="M@keskilled0"

userManagementArtifactPath="../build/contracts/userManagement.json"
blockchainServer="http://127.0.0.1:7545"

def connectWithContract(wallet,artifact=userManagementArtifactPath):
    web3=Web3(HTTPProvider(blockchainServer)) # it is connecting with server
    print('Connected with Blockchain Server')

    if (wallet==0):
        web3.eth.defaultAccount=web3.eth.accounts[0]
    else:
        web3.eth.defaultAccount=wallet
    print('Wallet Selected')

    with open(artifact) as f:
        artifact_json=json.load(f)
        contract_abi=artifact_json['abi']
        contract_address=artifact_json['networks']['5777']['address']
    
    contract=web3.eth.contract(abi=contract_abi,address=contract_address)
    print('Contract Selected')
    return contract,web3

@app.route('/')
def homePage():
    return render_template('index.html')

@app.route('/login')
def loginPage():
    return render_template('login.html')

@app.route('/signup')
def signupPage():
    return render_template('signup.html')

@app.route('/register',methods=['POST']) # page (1 Route), page (2 Route)
def register():
    wallet=request.form['address']
    username=request.form['username']
    email=request.form['email']
    password=request.form['password']
    confirmPassword=request.form['confirmPassword']
    
    if(password!=confirmPassword):
        return render_template('signup.html',message='passwords not matched, try again')
    
    contract,web3=connectWithContract(wallet) # UserManagement
    try:
        tx_hash=contract.functions.userSignUp(wallet,username,password,email).transact()
        web3.eth.waitForTransactionReceipt(tx_hash)
        print('Transaction Successful')
        return render_template('signup.html',message='Signup Successful')
    except:
        return render_template('signup.html',message='there is problem in creating account')

@app.route('/loginForm',methods=['POST'])
def loginForm():
    username=request.form['username']
    password=request.form['password']

    contract,web3=connectWithContract(0)
    try:
        result=contract.functions.userLogin(username,password).call()
        print(result)
        if result==True:
            response=contract.functions.viewUserByUsername(username).call()
            print (response)
            return render_template('login.html',message='success')
        else:
            return render_template('login.html',message='Invalid credentials')
    except Exception as e:
        return render_template('login.html',message='Invalid details of username')

@app.route('/logout')
def logout():
    session.clear()
    return redirect('/')

if __name__=="__main__":
    app.run(host='0.0.0.0',port=4001,debug=True)