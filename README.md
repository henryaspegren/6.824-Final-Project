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
            "nonce" : 2264682190,
            "value" : 682400000000000000
         }
      ],
      "timestamp" : 1524517805
   },
   "consensusData" : null,
   "data" : null,
   "height" : 1,
   "previousBlockId" : "0",
   "timestamp" : 1525999018
}
```

The above genesis block gives all the coins to one public key and enables it to be staked. In a real system you would have the coins widely distributed with many initial stakers.

### Putting it all together

## Becoming a staker

### Commit to an R-point

## Recovering from re-orgs

