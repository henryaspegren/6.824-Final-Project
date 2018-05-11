# ECTO/Cryptokernel

## Introduction 

ECTO is a proof-of-stake implementation designed to eliminate the risk of equivocation and be resistant to stake-grinding. In traditional PoS systems such as PeerCoin it is possible to produce blocks on any number of competing chains simultaneously (this is known as equivocation). It would be advantageous to do this because it reduces your risk of betting on the non-majority fork to zero and hence you have "nothing at stake". As a result there is no guarantee that the system can ever come to consensus. Additionally, in PeerCoin it is possible to permute the randomness used in selecting the next block proposer to your advantage so you are more likely to be selected to produce a block (known as "stake-grinding"). This effectively devolves staking into proof-of-work as those who can stake-grind the most will win the most blocks.

### Disincetivises equivocation

ECTO increases the cost of equivocation by enabling observers of equivocation to compute the equivocators private key if they are caught. This disincentivises equivocation as the risk of doing so is to forfeit all your money. ECTO achieves this by using committed R-point Schnorr signatures to sign each block produced. Each public key with staked coins commits to an R-point in order to stake, and must sign any subsequent block using that R-point. Once a block has been broadcast on the network signed with a particular R-point, that point can never be used again to sign or the private key can be computed. Therefore, the staker must replace the old R-point with a new one in the block they produce or by spending the output.

### Resistant to stake-grinding

ECTO resists stake grinding by using a randomness source from 1000 blocks prior to the current block. This increases the work required to stake-grind successfully as affecting the current block would require you to have staked block n - 1000, which is likely to be very difficult to achive reliably in practice if the stake pool is well distributed. 

## Bootstrapping a new ECTO/CK network

When boostraping an ECTO system it is important to ensure the initial stake is highly distributed across many entities. In PoS, 51% attacks are more dangerous than in PoW because they cannot be recovered from without hard forking. This also means care should be taken to ensure the initial distribution will not cause coins to become highly concentrated among a small number of entities. Finally, since PoS is a permissioned system, if there are no more stakeable coins in the system it will be impossible for any new blocks to be produced without a hard fork or deep re-org.

### Generate a new R-point

To generate a new R-point and private r-scalar one can use the utility in `utils`. Simply compile and run the executable and a new R/r pair will be produced. E.g. 

```
r: 
YDVGsTEyLaBMtEiNzSpXoRWQ+0MMb2d7f5nr0pVuUbw=

R: 
AocFe2BXTEA+5zhlgLG9yIIwssAYGaROQHKONKKMMW17
```

The R-point is public and should be published on the ledger to be associated with a public key for staking. The r-scaler should be saved in a file named `rpoint.key` in the root directory where `ckd` is located and kept private.  

### Generate a public/private key pair

Generate a new wallet account using the `ckd` command `account`. This will generate a new public/private key pair. Then use `dumpprivkeys` to retreive the raw private key for the newly created account. E.g.

```
$ ./ckd account mycoins
Please enter your wallet passphrase: ******************
{
	"balance" : "0",
	"keys" : 
	[
		{
			"privKey" : 
			{
				"cipherText" : "BgY23lAN9vgGETSsgu1BEASY+KV0u95aDjZ00JQrTQzg3K6G3oZHHLvBOe7RU8yf",
				"iv" : "hvX/Yrw5psWsAnYWBNSfLw==",
				"salt" : "oeEWV8fIHNAjMaY0bDMzl2jg8l0KbuwKG6IloDOFJP8="
			},
			"pubKey" : "BDsV9Pts0Gq/t2Q6m3vGH9RBa3eoOCtZKmCmFir3dZHSf+D7/kwtUpYDmLiI5pvPesFYTKANmu5xpRlFvEWruuM="
		}
	],
	"name" : "mycoins"
}

$ ./ckd dumpprivkeys mycoins
Please enter your wallet passphrase: ********
{
	"BDsV9Pts0Gq/t2Q6m3vGH9RBa3eoOCtZKmCmFir3dZHSf+D7/kwtUpYDmLiI5pvPesFYTKANmu5xpRlFvEWruuM=" : "oGKr6W0fI8NTl3BYMxAnQtHFBZlTUfRvjkVscKURyd8="
}


```

### Design the genesis block

```
{
   "coinbaseTx" : {
      "outputs" : [
         {
            "data" : {
               "publicKey" : "BPDwsnbf2mvbMkOB9/R8SoftQHjZjc62IS4+gEdfQggBN75ZxlWYwUZgh2BTkLSU06hLRpQAdRCR1C0TfKHpL2Q=",
               "rPointCommitment" : "Ag2I0Amz83j6poAOA2jpEEVyg//+HvT4dkAhn9NSGprG"
            },
            "nonce" : 0,
            "value" : 682400000000000000
         }
      ],
      "timestamp" : 1525999018
   },
   "consensusData" : null,
   "data" : null,
   "height" : 1,
   "previousBlockId" : "0",
   "timestamp" : 1525999018
}
```

The above genesis block gives all the coins to one public key and enables its coins to be staked. In a real system you would have the coins widely distributed with many initial stakers. Save the JSON of the genesis block in text file in the root directory where `ckd` is located.

### Putting it all together

All that's left is to specify your coin parameters in `config.json` as an object in the `coins` array. Here is an example specification: 

```
{
			"blockdb" : "./posblockdb",
			"consensus" : 
			{
				"params" : 
				{
					"blocktime" : 150,
					"ageWeight": "1",
					"amountWeight": "1"
				},
				"type" : "pos"
			},
			"genesisblock" : "posgenesisblock.json",
			"name" : "824Coin",
			"peerdb" : "./pospeers",
			"port" : 48000,
			"rpcport" : 8383,
			"subsidy" : "none",
			"walletdb" : "./posaddressesdb"
		}
```

The above example provides a negligible block reward so all coins should be emitted in the genesis block.

## Becoming a staker

Edit `config.json` and fill in the `pubKey` and `privKey` fields with your staking public and private key respectively. Then set `miner` to `true` and `ckd` will stake any stakeable outputs for the given `pubKey`. Before an output is stakeable it must have a committed R-point.  

### Commit to an R-point

In order to stake the coins associated with a given output, one has to create the output with a `rPointCommitment` data field. The script `stake.py` in `utils` provides example code to produce such an output. Edit the script to include your desired account to take outputs from, wallet password and RPC password and then run the script to issue a transaction.

## Recovering from re-orgs

If you were unfortunate enough to not bet on the longest chain and your block was orphaned, you will need to commit to a new R-point before you can stake again. 

## Using the demo coin 

Since you do not have any coins in the demo system you will be unable to stake, but you will be able to sychronise and validate the system state. Simply running `./ckd` from the command line will allow you to connect to peers and download the blocks. It may be useful to set `verbose` to `true` in `config.json` after first-run in order to see things happening on the screen.
