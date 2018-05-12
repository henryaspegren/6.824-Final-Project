#!/usr/bin/python3

import requests
import json
import base64
import time

rpcuser = "ckrpc"
rpcpassword = "vBsWgLPmgPHHygp/JCIgclKuXGGoCYLpzldGIJuUH40="
account = "mining"
walletpassword = "Con10t$d"
rpoint = "AocFe2BXTEA+5zhlgLG9yIIwssAYGaROQHKONKKMMW17"
pubkey = "BGOjpbmxzX26d7zHmNxy3RWb94MzTciGhF7y8ehF2EH2BlTDStCrAhSmmfmbaWDuRYqagRViAhVj6QhOsfp4oT4="
url = "http://localhost:8383"

def request(method, params):
    authStr = rpcuser + ":" +  rpcpassword
    encoded_str = str(base64.b64encode(authStr.encode()), 'utf-8')
    headers = {
        "content-type": "application/json",
        "Authorization": "Basic " + encoded_str}
    payload = {
        "method": method,
        "params": params,
        "jsonrpc": "2.0",
        "id": 0,
    }
    response = requests.post(url, data=json.dumps(payload), headers=headers).json()  
    return response

def initialSend():
    inp = request("listunspentoutputs", {"account": account})["result"]["outputs"].pop()

    tx = {"inputs":     [{"outputId": inp["id"], "data": {}}], 
          "outputs":    [{"value": inp["value"] - 10000000, 
                          "nonce": time.time(), 
                          "data": {"publicKey": pubkey,
                                   "rPointCommitment": rpoint}}], 
          "timestamp":  time.time()
          }     
          
    return request("signtransaction", {"transaction": tx, "password": walletpassword})["result"]

tx = initialSend()
print(request("sendrawtransaction", {"transaction": tx}))

