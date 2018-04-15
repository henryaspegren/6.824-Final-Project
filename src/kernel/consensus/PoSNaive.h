#ifndef POSNAIVE_H_INCLUDED
#define POSNAIVE_H_INCLUDED

#include "../blockchain.h"
#include <unordered_map>

namespace CryptoKernel {
/**
*	Implements a (Naive) Proof of Stake consensus Algorithm.	
*/
class PoSNaive : public CryptoKernel::Consensus {
public:
	PoSNaive(const uint64_t blockTarget, 
		Blockchain* blockchain,
		const bool run_miner, 
		const CryptoKernel::BigNum amountWeight, 
		const CryptoKernel::BigNum ageWeight, 
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

	void miner();
	
	void start();

private:
	// config params.... not sure where these should go
	// (these are fixed at start)
	CryptoKernel::BigNum amountWeight;
	CryptoKernel::BigNum ageWeight;
	uint64_t blockTarget;

	// member variables 
	Blockchain* blockchain;
	// TODO - wrong word? Should we call it staker?
	bool run_miner;
	// the pubkey we are mining for 
	std::string pubKey;
	// our staked outputs and associated amounts
	std::unordered_map<std::string, uint64_t> stakedOutputValues;
	
	/* Consensus critical data */	
	// need to keep track of when an output was last staked
	std::unordered_map<std::string, uint64_t> heightLastStaked;

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
	};

	ConsensusData getConsensusData(const CryptoKernel::Blockchain::block& block);

	ConsensusData getConsensusData(const CryptoKernel::Blockchain::dbBlock& block);

	Json::Value consensusDataToJson(const ConsensusData& cd);	 

	BigNum calculateStakeConsumed(const uint64_t age, const uint64_t amount);

	BigNum calculateTarget(Storage::Transaction* transaction, const CryptoKernel::BigNum& prevBlockId);

	BigNum selectionFunction(const CryptoKernel::BigNum& stakeConsumed, const CryptoKernel::BigNum& blockId, const uint64_t timestamp, const std::string& outputId);
	
};

}

#endif  
