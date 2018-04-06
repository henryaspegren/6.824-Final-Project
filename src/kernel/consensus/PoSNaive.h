#ifndef POSNAIVE_H_INCLUDED
#define POSNAIVE_H_INCLUDED

#include "blockchain.h"

namespace CryptoKernel {
class Consensus::PoSNaive : public Consensus { 
public:
	PoSNaive();
	
	bool isBlockBetter(Storage::Transaction* transaction,
			const CryptoKernel::Blockchain::block& block,
			const CryptoKernel::Blockchain::dbBlock& tip);

	bool checkConsensusRules(Storage::Transaction* transaction,
				const CryptoKernel::Blockchain::block& block,
				const CryptoKernel::Blockchain::dbBlock& previousBlock);	

	Json::Value generateConsensusData(Storage::Transaction* transaction, 
				const CryptoKernel::BigNum& previousBlockId,
				const std::string& publicKey);
	
	bool verifyTransaction(Storage::Transaction *transaction, 
				const CryptoKernel::Blockchain::transaction& tx);

	bool confirmTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	bool submitTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	bool submitBlock(Storage::Transaction *transaction, 
				const CryptoKernel::Blockchain::block& block);

	void start();

private:
	// config params.... not sure where these should go
	uint64_t amountWeight;
	uint64_t heightWeight;

	std::string pubKey

	// a staked coin
	struct StakedCoin{
		uint64_t amount;
		uint64_t height;
	}
	
	// the stake associated with a given pubKey
	// staked coins are added and removed over time 	
	struct Stake{
		uint64_t lastStakedHeight; 
		std::unordered_map<uint64_t, StakedCoins> stakedCoins;
	}

	// pubKeys have stakes that evolve overtime
	std::unorderd_map<std::string, Stake> pubKeyStakes;
	
	struct ConsensusData{
		BigNum totalStakedCoinsConsumed;
	}

	std::unorderd_map<std::string, Stake> pubKeyStakes;
	 
	
	uint64_t calculateCoinAge(const StakedCoin& sc, 
					const uint64_t amountWeight,
					const  uint64_t heightWeight, 
					const uint64_t currentHeight);

	uint64_t calculateTotalCoinAge(const Stake& stake);

	
};
	
} 
