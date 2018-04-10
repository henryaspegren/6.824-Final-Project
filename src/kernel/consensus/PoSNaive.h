#ifndef POSNAIVE_H_INCLUDED
#define POSNAIVE_H_INCLUDED

#include "blockchain.h"

namespace CryptoKernel {
class Consensus::PoSNaive : public Consensus { 
public:
	PoSNaive(const uint64_t amountWeight, 
		const uint64_t ageWeight, 
		std::string pubkey);
	
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
	
	// in PoSNaive this needs to update the stake pool
	// since outputs are automatically staked
	bool confirmTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	bool submitTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	// in PoSNaive this needs to update the stake pool
	// to reflect that a stake has been consumed
	bool submitBlock(Storage::Transaction *transaction, 
				const CryptoKernel::Blockchain::block& block);

	void start();

private:
	// config params.... not sure where these should go
	// (these are fixed at start)
	uint64_t amountWeight;
	uint64_t ageWeight;

	std::string pubKey;
	
	/* Consensus critical data */	
	// need to keep track of when an output was last staked
	std::unorderd_map<std::string, uint64_t> heightLastStaked;

	/* Consensus data that is actually stored in the blockchain */	
	struct ConsensusData{
		CryptoKernel::BigNum stakeConsumed;
		CryptoKernel::BigNum target;
		CryptoKernel::BigNum totalWork;
		CryptoKernel::BigNum totalStakeConsumed;
		std::string pubKey;
		std::string outputId;
		uint64_t timestamp;
		std::string signature; 
	}

	ConsensusData getConsensusData(const CryptoKernel::Blockchain::block& block);

	ConsensusData getConesnsusData(const CryptoKernel::Blockchain::dbBlock& dbBlock);

	Json::Value consensusDataToJson(const ConsensusData& cd);	 

	CryptoKernel::BigNum calculateStakeConsumed(const uint64_t age, const uint64_t amount);

	CrytoKernel::BigNum calculateTarget(Storage::Transaction* transaction, const CryptoKernel::BigNum& prevBlockId);	
};
	
	// TODO @James - we need some way to adjust the hash so that it gets smaller
	// the bigger your stake (so we are more likely to select that stake)
	CrpytoKernel::BigNum selectionFunction(const CryptoKernel::BigNum& stakeConsumed, const CrpytoKernel::BigNum& consumableStake, const std::string& blockId, const std::string& timestamp, const std::string& outputId);
	
}

 
