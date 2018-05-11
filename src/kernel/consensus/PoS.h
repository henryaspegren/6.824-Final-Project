#ifndef POS_H_INCLUDED
#define POS_H_INCLUDED

#include <unordered_map>
#include <thread>

#include "../../cschnorr/context.h"

#include "blockchain.h"

namespace CryptoKernel {
/**
*	Implements a Proof of Stake consensus Algorithm.
*	The block proposer is selected based on a lottery
*	in which the probability of wining increases with
*	the amount and age of coins.
*/
class Consensus::PoS : public Consensus {
public:
	PoS(const uint64_t blockTarget,
		Blockchain* blockchain,
		const bool run_miner,
		const CryptoKernel::BigNum& amountWeight,
		const CryptoKernel::BigNum& ageWeight,
		const std::string& pubkey,
		const std::string& privKey);

	~PoS();

	bool isBlockBetter(Storage::Transaction* transaction,
			const CryptoKernel::Blockchain::block& block,
			const CryptoKernel::Blockchain::dbBlock& tip);

	bool checkConsensusRules(Storage::Transaction* transaction,
				CryptoKernel::Blockchain::block& block,
				const CryptoKernel::Blockchain::dbBlock& previousBlock);

	Json::Value generateConsensusData(Storage::Transaction* transaction,
				const CryptoKernel::BigNum& previousBlockId,
				const std::string& publicKey);

	bool verifyTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	// in PoS special staking transactions are required
	// to enter the stake pool
	bool confirmTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	bool submitTransaction(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::transaction& tx);

	// in PoS this is required to update
	// the stake pool as a block is staked
	bool submitBlock(Storage::Transaction *transaction,
				const CryptoKernel::Blockchain::block& block);

	void reverseBlock(Storage::Transaction *transaction);

	void miner();

	void start();

private:
	// config parameters (taken from the config.json file)
	CryptoKernel::BigNum amountWeight;
	CryptoKernel::BigNum ageWeight;
	uint64_t blockTarget;

	// member variables
	Blockchain* blockchain;
	// in PoS the "mining" is just checking to see
	// if a staked output has been selected to propose
	// a block - this consumes minimal energy and
	// you cannot gain a competitive advantage
	// through more computational resources
	bool run_miner;
	std::unique_ptr<std::thread> minerThread;
	// the pubKey we use to get staked outputs
	std::string pubKey;

	std::string privKey;

	schnorr_context* ctx;

	// stake state = [output height last staked,
	//		  R point commitment (equivocation protection),
	//	        	can be staked]
	// the following functions are wrappers around stake state,
	// which is stored in the database
	std::tuple<uint64_t, std::string, bool> getStakeState(Storage::Transaction* transaction,
	                             			 const std::string& outputId);
	void setStakeState(Storage::Transaction* transaction,
					   const std::string& outputId,
					   const std::tuple<uint64_t, std::string, bool>& stakeState);
	void eraseStakeState(Storage::Transaction* transaction,
					     const std::string& outputId);

	/* Consensus data that is actually stored in the blockchain */
	struct ConsensusData{
		CryptoKernel::BigNum stakeConsumed;
		CryptoKernel::BigNum target;
		// The totalWork here is not the same as in PoW
		// it reflects the total 'stake age consumed'
		// rather than the hash power of the network
		CryptoKernel::BigNum totalWork;
		std::string pubKey;
		std::string outputId;
		uint64_t outputHeightLastStaked;
		uint64_t outputValue;
		std::string currentRPointCommitment;
		uint64_t timestamp;
		// when a block is mined, the staker
		// has the option of commiting to another
		// R point and remaining in the stake pool
		std::string newRPointCommitment;
		// the signature must reflect the committed
		// R point in the stake pool
		std::string signature;
	};

	ConsensusData getConsensusData(const CryptoKernel::Blockchain::block& block);

	ConsensusData getConsensusData(const CryptoKernel::Blockchain::dbBlock& block);

	Json::Value consensusDataToJson(const ConsensusData& cd);

	BigNum calculateStakeConsumed(const uint64_t age, const uint64_t amount);

	BigNum calculateTarget(Storage::Transaction* transaction, const CryptoKernel::BigNum& prevBlockId);

	BigNum calculateHash(const CryptoKernel::BigNum& blockId, const uint64_t timestamp, const std::string& outputId);

};

}

#endif // POS_H_INCLUDED
