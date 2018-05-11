#include <sstream>
#include <math.h>
#include <iostream>

#include <cschnorr/signature.h>

#include "PoS.h"
#include "base64.h"
#include "../crypto.h"

CryptoKernel::Consensus::PoS::PoS(const uint64_t blockTarget,
	CryptoKernel::Blockchain* blockchain, const bool run_miner,
	const CryptoKernel::BigNum& amountWeight,
	const CryptoKernel::BigNum& ageWeight, const std::string& pubKey,
	const std::string& privKey) {
	this->blockTarget = blockTarget;
	this->blockchain = blockchain;
	this->run_miner = run_miner;
	this->amountWeight = amountWeight;
	this->ageWeight = ageWeight;
	this->pubKey = pubKey;
	this->privKey = privKey;
	this->ctx = schnorr_context_new();
};

CryptoKernel::Consensus::PoS::~PoS(){
	this->run_miner = false;
	this->minerThread->join();
	schnorr_context_free(this->ctx);
};

bool CryptoKernel::Consensus::PoS::isBlockBetter(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::block& block,
	const CryptoKernel::Blockchain::dbBlock& tip){
	const CryptoKernel::Consensus::PoS::ConsensusData blockData = this->getConsensusData(block);
	const CryptoKernel::Consensus::PoS::ConsensusData tipData = this->getConsensusData(tip);
	return  blockData.totalWork > tipData.totalWork;
};

bool CryptoKernel::Consensus::PoS::checkConsensusRules(Storage::Transaction* transaction,
	CryptoKernel::Blockchain::block& block,
	const CryptoKernel::Blockchain::dbBlock& previousBlock){
	try{
		const CryptoKernel::Consensus::PoS::ConsensusData blockData = CryptoKernel::Consensus::PoS::getConsensusData(block);
		const CryptoKernel::Blockchain::output output = this->blockchain->getOutput(transaction, blockData.outputId);
		const uint64_t outputValue = output.getValue();
		const Json::Value outputData = output.getData();

		const auto stakeState = this->getStakeState(transaction, blockData.outputId);

		const uint64_t outputHeightLastStaked = std::get<0>(stakeState);
		const std::string outputRPointCommitment = std::get<1>(stakeState);
		const bool outputCanBeStaked = std::get<2>(stakeState);
		CryptoKernel::BigNum blockId = block.getId();
		
		// we use 1000 blocks back if possible to make stake grinding 
		// more difficult 
		if(block.getHeight() > 1000) {
			blockId = this->blockchain->getBlockByHeightDB(transaction, block.getHeight() - 1000).getId();
		}

		// check that the output can be staked
		if( !outputCanBeStaked ){
			return false;
		}

		// check that the winning pubkey owns the output
		if( outputData["publicKey"].asString() != blockData.pubKey ){
			return false;
		}

		// check that the output height last staked, R point commitment and value are correct
		if( outputValue != blockData.outputValue || outputHeightLastStaked != blockData.outputHeightLastStaked || outputRPointCommitment != blockData.currentRPointCommitment) {
			return false;
		}

		// check that the stake is calculated correctly
		const uint64_t age = block.getHeight() - outputHeightLastStaked;
		if( age <= 0 ){
			return false;
		}
		CryptoKernel::BigNum stakeConsumed = this->calculateStakeConsumed(age, outputValue);
		if( stakeConsumed != blockData.stakeConsumed ){
			return false;
		}

		// check that the target is calculated corrctly
		CryptoKernel::BigNum target = this->calculateTarget(transaction, block.getPreviousBlockId());
		if( target  != blockData.target ){
			return false;
		}

		// verify the stake was selected according to the coin-age adjusted target
		CryptoKernel::BigNum hash = this->calculateHash(blockId, block.getTimestamp(), blockData.outputId);
		if( hash > target * stakeConsumed){
			return false;
		}

		const CryptoKernel::Consensus::PoS::ConsensusData prevBlockData = CryptoKernel::Consensus::PoS::getConsensusData(previousBlock);
		CryptoKernel::BigNum inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - target;
		// verify that the total work is updated correctly
		if( blockData.totalWork != ( inverse + prevBlockData.totalWork ) ){
			return false;
		}

		// now verify the R point commits to 
		// the pubKey that produced this block  
		
		committed_r_pubkey pub;
		pub.A = EC_POINT_new(ctx->group);
		pub.R = EC_POINT_new(ctx->group);
		const auto decodedR = base64_decode(blockData.currentRPointCommitment);
		if(decodedR.size() != 33) {
			return false;
		}

		if(EC_POINT_oct2point(ctx->group, pub.R, reinterpret_cast<const unsigned char*>(decodedR.c_str()), 33, ctx->bn_ctx) != 1) {
			return false;
		}

		const auto decodedPub = base64_decode(blockData.pubKey);
		if(decodedPub.size() != 65) {
			return false;
		}

		if(EC_POINT_oct2point(ctx->group, pub.A, reinterpret_cast<const unsigned char*>(decodedPub.c_str()), 65, ctx->bn_ctx) != 1) {
			return false;
		}

		committed_r_sig sig;
		sig.s = BN_new();

		const auto decodedS = base64_decode(blockData.signature);
		if(decodedS.size() != 32) {
			return false;
		}

		if(BN_bin2bn(reinterpret_cast<const unsigned char*>(decodedS.c_str()), 32, sig.s) == NULL) {
			return false;
		}

		if(committed_r_verify(ctx, &sig, &pub, reinterpret_cast<const unsigned char*>(hash.toString().c_str()), hash.toString().size()) != 1) {
			return false;
		}

		BN_free(sig.s);
		EC_POINT_free(pub.A);
		EC_POINT_free(pub.R);

		return true;
	} catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
		return false;
	} catch(const CryptoKernel::Blockchain::NotFoundException& e) {
		return false;
	}
}

Json::Value CryptoKernel::Consensus::PoS::generateConsensusData(Storage::Transaction* transaction,
	const CryptoKernel::BigNum& previousBlockId,
	const std::string& publicKey){
	CryptoKernel::Consensus::PoS::ConsensusData cd;
	cd.target = CryptoKernel::Consensus::PoS::calculateTarget(transaction, previousBlockId);
	Json::Value asJson = CryptoKernel::Consensus::PoS::consensusDataToJson(cd);
	return asJson;
};

void CryptoKernel::Consensus::PoS::miner(){
	while (run_miner) {
		CryptoKernel::Blockchain::block block = this->blockchain->generateVerifyingBlock(pubKey);
		CryptoKernel::Blockchain::dbBlock previousBlock = this->blockchain->getBlockDB(
                        block.getPreviousBlockId().toString());
		uint64_t height = block.getHeight();


		std::set<uint64_t> blockTimes;
		CryptoKernel::Blockchain::block currentBlock = this->blockchain->getBlock(
					block.getPreviousBlockId().toString());
		for(uint64_t h = height - 1; h >= 1 && height - 1 - h < 13; h--) {
			blockTimes.emplace(currentBlock.getTimestamp());
			if(h > 1) {
				currentBlock = this->blockchain->getBlockByHeight(h);
			}
		}

		auto it = blockTimes.begin();
		std::advance(it, blockTimes.size() / 2);
		const uint64_t medianTime = *it;

		uint64_t time2 = medianTime + 1;

		Json::Value consensusDataThisBlock = block.getConsensusData();
		Json::Value consensusDataPreviousBlock = previousBlock.getConsensusData();
		CryptoKernel::BigNum totalWorkPrev = CryptoKernel::BigNum(
			consensusDataPreviousBlock["totalWork"].asString());
		CryptoKernel::BigNum target = CryptoKernel::BigNum(consensusDataThisBlock["target"].asString());
		bool blockMined = false;
		CryptoKernel::BigNum previousBlockId = block.getPreviousBlockId();
		do{
			if(time2 > medianTime + 2.5 * 60 * 60) {
				std::cout << "Exhausted timestamps\n";
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				break;
			}

			block.setTimestamp(time2);

			const auto outputs = this->blockchain->getUnspentOutputs(pubKey);

			// check to see if any of our staked outputs are selected
			for( auto const& entry :  outputs ){
				// only check the outputs that can be staked
				const std::string outputId = entry.getId().toString();

				std::unique_ptr<Storage::Transaction> dbTx(this->blockchain->getTxHandle());
				const auto stakeState = getStakeState(dbTx.get(), outputId);
				bool canBeStaked = std::get<2>(stakeState);
				dbTx.reset();

				if( !canBeStaked ) {
					continue;
				}

				CryptoKernel::BigNum blockId = block.getId();
				if(block.getHeight() > 1000) {
					blockId = this->blockchain->getBlockByHeight(block.getHeight() - 1000).getId();
				}

				uint64_t value = entry.getValue();
				uint64_t outputHeightLastStaked = std::get<0>(stakeState);
				std::string rPointCommitment = std::get<1>(stakeState);
				uint64_t age = height - outputHeightLastStaked;
				CryptoKernel::BigNum stakeConsumed =
					this->calculateStakeConsumed(age, value);
				CryptoKernel::BigNum hash =
					this->calculateHash(blockId, time2, outputId);
				// output selected
				if( hash <= target * stakeConsumed) {
					consensusDataThisBlock["stakeConsumed"] = stakeConsumed.toString();
					consensusDataThisBlock["target"] = (target).toString();
					consensusDataThisBlock["totalWork"] = (CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - (target) + totalWorkPrev).toString();
					consensusDataThisBlock["pubKey"] = pubKey;
					consensusDataThisBlock["outputId"] = outputId;
					consensusDataThisBlock["outputHeightLastStaked"] = outputHeightLastStaked;
					consensusDataThisBlock["outputValue"] = value;
					consensusDataThisBlock["currentRPointCommitment"] = rPointCommitment;
					consensusDataThisBlock["timestamp"] = time2;

					// Generate new R point
					committed_r_key* newKey = committed_r_key_new(ctx);

					// Copy our private key
					const auto decodedA = base64_decode(privKey);
					if(BN_bin2bn(reinterpret_cast<const unsigned char*>(decodedA.c_str()), 32, newKey->a) == NULL) {
						throw std::runtime_error("Error decoding private key");
					}

					// Save new point
					std::ifstream f;
					f.open("rpoint.key");
					std::string k((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
					f.close();

					const auto decodedK = base64_decode(k);
					if(decodedK.size() != 32) {
						throw std::runtime_error("Failure decoding k value");
					}

					unsigned char* newk = new unsigned char[32];
					if(BN_bn2binpad(newKey->k, newk, 32) != 32) {
						throw std::runtime_error("Failure encoding k value");
					}

					std::ofstream of;
					of.open("rpoint.key");
					of << base64_encode(newk, 32);
					of.close();

					delete[] newk;

					unsigned char newR[33];
                    if(EC_POINT_point2oct(ctx->group, newKey->pub->R, POINT_CONVERSION_COMPRESSED, reinterpret_cast<unsigned char*>(&newR), 33, ctx->bn_ctx) != 33) {
						throw std::runtime_error("Failure encoding R point");
                    }

					consensusDataThisBlock["newRPointCommitment"] = base64_encode(reinterpret_cast<unsigned char*>(&newR), 33);
					const auto decodedOldK = base64_decode(rPointCommitment);

					if(EC_POINT_oct2point(ctx->group, newKey->pub->R, reinterpret_cast<const unsigned char*>(decodedOldK.c_str()), 33, ctx->bn_ctx) != 1) {
						throw std::runtime_error("Failure decoding R point");
					}

					if(BN_bin2bn(reinterpret_cast<const unsigned char*>(decodedK.c_str()), 32, newKey->k) == NULL) {
						throw std::runtime_error("Failure decoding k value");
					}

					committed_r_sig* sig;
					if(committed_r_sign(ctx, &sig, newKey, reinterpret_cast<const unsigned char *>(hash.toString().c_str()), hash.toString().size()) != 1) {
						throw std::runtime_error("Failure signing block");
					}

					unsigned char* s = new unsigned char[32];
					size_t val = BN_bn2binpad(sig->s, s, 32);
					if(val != 32) {
						throw std::runtime_error("Failure encoding signature " + std::to_string(BN_num_bytes(sig->s)));
					}

					consensusDataThisBlock["signature"] = base64_encode(s, 32);

					delete[] s;
					committed_r_sig_free(sig);
					committed_r_key_free(newKey);

					block.setConsensusData(consensusDataThisBlock);
					this->blockchain->submitBlock(block);
					blockMined = true;
				}
			}

			time2++;
		} while( run_miner && !blockMined );
	};
};


bool CryptoKernel::Consensus::PoS::verifyTransaction(Storage::Transaction *transaction,
			const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::Consensus::PoS::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx){
	const std::set<CryptoKernel::Blockchain::input> inputs = tx.getInputs();
	const std::set<CryptoKernel::Blockchain::output> outputs = tx.getOutputs();
	uint64_t height = 1;
	try {
		CryptoKernel::Blockchain::block block = this->blockchain->getBlock(transaction, "tip");
		height = block.getHeight()+1;
	} catch (const CryptoKernel::Blockchain::NotFoundException& e) {
		// height of the genesis block, which
		// has no "tip" should be 1 so make sure this
		// edge case is handled correctly
	}

	// mark all outputids so they cannot be staked again
	// BUT keep the heightLastStaked and R point
	// commitment incase a reorg requires
	// us to re-stake them or if they equivocate!
	for( const auto& input : inputs ) {
		CryptoKernel::BigNum outputId = input.getOutputId();
		const auto res = getStakeState(transaction, outputId.toString());
		this->setStakeState(transaction, outputId.toString(),
					std::make_tuple(std::get<0>(res),
							std::get<1>(res),
							false));
	}

	// add all outputids that have been created, recording height
	for( const auto& output : outputs ) {
		CryptoKernel::BigNum outputId = output.getId();
		Json::Value data = output.getData();
		if(data["publicKey"].isString() && data["rPointCommitment"].isString()) {
			std::string outputPubKey = data["publicKey"].asString();
			std::string rPointCommitment = data["rPointCommitment"].asString();
			setStakeState(transaction, outputId.toString(), std::make_tuple(height, rPointCommitment, true));
		}
	}

	return true;
};

bool CryptoKernel::Consensus::PoS::submitTransaction(Storage::Transaction *transaction, const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::Consensus::PoS::submitBlock(Storage::Transaction *transaction, const CryptoKernel::Blockchain::block& block){
	CryptoKernel::Consensus::PoS::ConsensusData blockData = CryptoKernel::Consensus::PoS::getConsensusData(block);
	// update the stake height and R point
	uint64_t newHeightLastStaked = block.getHeight();
	std::string newRPointCommitment = blockData.newRPointCommitment;
	// output may have been spent, so we still do a DB lookup
	// to ensure that the stake state reflects this
	const auto res = getStakeState(transaction, blockData.outputId);
	const auto newState = std::make_tuple(newHeightLastStaked,
					newRPointCommitment,
					std::get<2>(res));
	setStakeState(transaction, blockData.outputId, newState);

	return true;
};

void CryptoKernel::Consensus::PoS::reverseBlock(Storage::Transaction *transaction){
	const CryptoKernel::Blockchain::block& tip = this->blockchain->getBlock(transaction, "tip");
	const Json::Value consensusDataJson = tip.getConsensusData();
	// first "unstake" the staked output that was orphaned
	// TODO - this is where we can add the code to catch equivocation
	this->setStakeState(transaction,
				  consensusDataJson["outputId"].asString(),
				  std::make_tuple(consensusDataJson["outputHeightLastStaked"].asUInt64(),
						  consensusDataJson["currentRPointCommitment"].asString(), true));

	std::set<CryptoKernel::Blockchain::transaction> transactions = tip.getTransactions();
	for( const auto& tx : transactions ) {
		std::set<CryptoKernel::Blockchain::input> inputs = tx.getInputs();
		std::set<CryptoKernel::Blockchain::output> outputs = tx.getOutputs();
		// since the outputs are no longer spent
		// mark them as stakeable
		for( const auto& input : inputs ) {
			const std::string outputId = input.getOutputId().toString();
			const auto res = this->getStakeState(transaction, outputId);
			this->setStakeState(transaction, outputId, std::make_tuple(std::get<0>(res), std::get<1>(res), false));
		}
		// newly created outputs no longer exist
		// so remove them entirely
		for( const auto& output : outputs ){
			const std::string outputId = output.getId().toString();
			this->eraseStakeState(transaction, outputId);
		}
	}

};

void CryptoKernel::Consensus::PoS::start(){
	this->minerThread.reset(new std::thread(&CryptoKernel::Consensus::PoS::miner, this));
};

CryptoKernel::Consensus::PoS::ConsensusData CryptoKernel::Consensus::PoS::getConsensusData(const CryptoKernel::Blockchain::block& block){
	CryptoKernel::Consensus::PoS::ConsensusData cd;
	const Json::Value cj = block.getConsensusData();
	try {
		cd.stakeConsumed = CryptoKernel::BigNum(cj["stakeConsumed"].asString());
		cd.target = CryptoKernel::BigNum(cj["target"].asString());
		cd.totalWork = CryptoKernel::BigNum(cj["totalWork"].asString());
		cd.pubKey = cj["pubKey"].asString();
		cd.outputId = cj["outputId"].asString();
		cd.outputHeightLastStaked  = cj["outputHeightLastStaked"].asUInt64();
		cd.outputValue = cj["outputValue"].asUInt64();
		cd.currentRPointCommitment = cj["currentRPointCommitment"].asString();
		cd.timestamp = cj["timestamp"].asUInt64();
		cd.newRPointCommitment = cj["newRPointCommitment"].asString();
		cd.signature = cj["signature"].asString();
	} catch(const Json::Exception& e) {
		throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
	}
	return cd;
};

CryptoKernel::Consensus::PoS::ConsensusData CryptoKernel::Consensus::PoS::getConsensusData(const CryptoKernel::Blockchain::dbBlock& block){
        CryptoKernel::Consensus::PoS::ConsensusData cd;
        const Json::Value cj = block.getConsensusData();
        try {
                cd.stakeConsumed = CryptoKernel::BigNum(cj["stakeConsumed"].asString());
		cd.target = CryptoKernel::BigNum(cj["target"].asString());
                cd.totalWork = CryptoKernel::BigNum(cj["totalWork"].asString());
		cd.pubKey = cj["pubKey"].asString();
                cd.outputId = cj["outputId"].asString();
		cd.outputHeightLastStaked = cj["outputHeightLastStaked"].asUInt64();
		cd.outputValue = cj["outputValue"].asUInt64();
		cd.currentRPointCommitment = cj["currentRPointCommitment"].asString();
		cd.timestamp = cj["timestamp"].asUInt64();
		cd.newRPointCommitment = cj["newRPointCommitment"].asString();
		cd.signature = cj["signature"].asString();
	 } catch(const Json::Exception& e) {
                throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
        }
        return cd;
};

Json::Value CryptoKernel::Consensus::PoS::consensusDataToJson(const CryptoKernel::Consensus::PoS::ConsensusData& cd){
	Json::Value consensusDataAsJson;
	consensusDataAsJson["stakeConsumed"] = cd.stakeConsumed.toString();
	consensusDataAsJson["target"] = cd.target.toString();
	consensusDataAsJson["totalWork"] = cd.totalWork.toString();
	consensusDataAsJson["pubKey"] = cd.pubKey;
	consensusDataAsJson["outputId"] = cd.outputId;
	consensusDataAsJson["outputHeightLastStaked"] = cd.outputHeightLastStaked;
	consensusDataAsJson["outputValue"] = cd.outputValue;
	consensusDataAsJson["currentRPointCommitment"] = cd.currentRPointCommitment;
	consensusDataAsJson["timestamp"] = cd.timestamp;
	consensusDataAsJson["newRPointCommitment"] = cd.newRPointCommitment;
	consensusDataAsJson["signature"] = cd.signature;
	return consensusDataAsJson;
};

CryptoKernel::BigNum CryptoKernel::Consensus::PoS::calculateStakeConsumed(
        const uint64_t age, const uint64_t amount){
	std::stringstream buffer;
	buffer << std::hex << age;
	CryptoKernel::BigNum bigAge = CryptoKernel::BigNum(buffer.str());
	std::stringstream buffer2;
	buffer2 << std::hex << amount;
	CryptoKernel::BigNum bigAmount = CryptoKernel::BigNum(buffer2.str());
	CryptoKernel::BigNum coinAge = bigAmount*this->amountWeight * bigAge*this->ageWeight;
	return coinAge;
};

CryptoKernel::BigNum CryptoKernel::Consensus::PoS::calculateTarget(Storage::Transaction* transaction,
	const CryptoKernel::BigNum& previousBlockId){
	// PoW difficulty function unchanged
	const uint64_t minBlocks = 144;
	const uint64_t maxBlocks = 4032;
    	const CryptoKernel::BigNum minDifficulty =
        	CryptoKernel::BigNum("fffffffffffffffffffffffffffffffffffffffffffffffffffff");
   	CryptoKernel::Blockchain::dbBlock currentBlock = blockchain->getBlockDB(transaction,
        	    previousBlockId.toString());
    	CryptoKernel::Consensus::PoS::ConsensusData currentBlockData =
		this->getConsensusData(currentBlock);
   	CryptoKernel::Blockchain::dbBlock lastSolved = currentBlock;
 	if(currentBlock.getHeight() < minBlocks) {
        	return minDifficulty;
   	} else if(currentBlock.getHeight() % 12 != 0) {
        	return currentBlockData.target;
   	} else {
        	uint64_t blocksScanned = 0;
      		CryptoKernel::BigNum difficultyAverage = CryptoKernel::BigNum("0");
        	CryptoKernel::BigNum previousDifficultyAverage = CryptoKernel::BigNum("0");
        	int64_t actualRate = 0;
        	int64_t targetRate = 0;
	        double rateAdjustmentRatio = 1.0;
       		double eventHorizonDeviation = 0.0;
        	double eventHorizonDeviationFast = 0.0;
        	double eventHorizonDeviationSlow = 0.0;
  	for(unsigned int i = 1; currentBlock.getHeight() != 1; i++) {
        	if(i > maxBlocks) {
                	break;
           	 }
           	 blocksScanned++;
           	 if(i == 1) {
                	difficultyAverage = currentBlockData.target;
           	 } else {
               		 std::stringstream buffer;
                	buffer << std::hex << i;
                	difficultyAverage = ((currentBlockData.target - previousDifficultyAverage) /
                                     CryptoKernel::BigNum(buffer.str())) + previousDifficultyAverage;
           	 }
            	previousDifficultyAverage = difficultyAverage;
            	actualRate = lastSolved.getTimestamp() - currentBlock.getTimestamp();
            	targetRate = blockTarget * blocksScanned;
            	rateAdjustmentRatio = 1.0;
            	if(actualRate < 0) {
                	actualRate = 0;
            	}
            	if(actualRate != 0 && targetRate != 0) {
                	rateAdjustmentRatio = double(targetRate) / double(actualRate);
            	}
            	eventHorizonDeviation = 1 + (0.7084 * pow((double(blocksScanned)/double(minBlocks)),
                                         -1.228));
            	eventHorizonDeviationFast = eventHorizonDeviation;
            	eventHorizonDeviationSlow = 1 / eventHorizonDeviation;
           	if(blocksScanned >= minBlocks) {
                	if((rateAdjustmentRatio <= eventHorizonDeviationSlow) ||
                        (rateAdjustmentRatio >= eventHorizonDeviationFast)) {
                 	   break;
               		}
           	}
            	if(currentBlock.getHeight() == 1) {
                	break;
            	}
            	currentBlock = blockchain->getBlockDB(transaction,
                                                  currentBlock.getPreviousBlockId().toString());
            	currentBlockData = this->getConsensusData(currentBlock);
 	 }
        CryptoKernel::BigNum newTarget = difficultyAverage;
        if(actualRate != 0 && targetRate != 0) {
       		 std::stringstream buffer;
            	buffer << std::hex << actualRate;
            	newTarget = newTarget * CryptoKernel::BigNum(buffer.str());
            	buffer.str("");
           	buffer << std::hex << targetRate;
            	newTarget = newTarget / CryptoKernel::BigNum(buffer.str());
        }
        if(newTarget > minDifficulty) {
           	 newTarget = minDifficulty;
        }
        return newTarget;
    }
};

CryptoKernel::BigNum CryptoKernel::Consensus::PoS::calculateHash(const CryptoKernel::BigNum& blockId, const uint64_t timestamp, const std::string& outputId){
	std::stringstream buffer;
	// TODO - use blockId from 100 blocks ago
	// 		to prevent stake grinding
	buffer << blockId.toString() << timestamp << outputId;
	CryptoKernel::Crypto crypto;
	return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
};

std::tuple<uint64_t, std::string, bool>
CryptoKernel::Consensus::PoS::getStakeState(Storage::Transaction* transaction,
	                             			 	 const std::string& outputId) {
	const Json::Value res = this->blockchain->consensusGet(transaction, outputId);
	if(!res.isArray()) {
		// if not in the database - not stakeable
		return std::make_tuple<uint64_t, std::string, bool> (0, "", false);
	} else {
		return std::make_tuple<uint64_t, std::string, bool> (res[0].asUInt64(), res[1].asString(), res[2].asBool());
	}
};

void CryptoKernel::Consensus::PoS::setStakeState(Storage::Transaction* transaction,
							 						  const std::string& outputId,
							 						  const std::tuple<uint64_t, std::string, bool>& stakeState) {
	Json::Value payload;
	payload.append(std::get<0>(stakeState));
	payload.append(std::get<1>(stakeState));
	payload.append(std::get<2>(stakeState));
	this->blockchain->consensusPut(transaction, outputId, payload);
};

void CryptoKernel::Consensus::PoS::eraseStakeState(Storage::Transaction* transaction, const std::string& outputId) {
	this->blockchain->consensusErase(transaction, outputId);
};
