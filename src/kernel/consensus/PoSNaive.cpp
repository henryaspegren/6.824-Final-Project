#include <sstream>
#include <math.h>

#include "PoSNaive.h"
#include "../crypto.h"

CryptoKernel::PoSNaive::PoSNaive(const uint64_t blockTarget,
	CryptoKernel::Blockchain* blockchain, const bool miner, const CryptoKernel::BigNum amountWeight, const CryptoKernel::BigNum ageWeight, std::string pubKey){
	this->blockTarget = blockTarget;
	this->blockchain = blockchain;
	this->miner = miner;
	this->amountWeight = amountWeight;
	this->ageWeight = ageWeight;
	this->pubKey = pubKey;
};

bool CryptoKernel::PoSNaive::isBlockBetter(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::block& block, 
	const CryptoKernel::Blockchain::dbBlock& tip){
	const CryptoKernel::PoSNaive::ConsensusData blockData = this->getConsensusData(block);
	const CryptoKernel::PoSNaive::ConsensusData tipData = this->getConsensusData(tip);	
	return  blockData.totalStakeConsumed > tipData.totalStakeConsumed;
};

bool CryptoKernel::PoSNaive::checkConsensusRules(Storage::Transaction* transaction, 
	const CryptoKernel::Blockchain::block& block,
	const CryptoKernel::Blockchain::dbBlock& previousBlock){
	try{ 
		const CryptoKernel::PoSNaive::ConsensusData blockData = CryptoKernel::PoSNaive::getConsensusData(block); 
		const CryptoKernel::Blockchain::output output = this->blockchain->getOutput(blockData.outputId);
		const uint64_t outputValue = output.getValue();
		const Json::Value outputData = output.getData();
		const CryptoKernel::BigNum blockId = block.getId();
	
		// check that the winning pubkey owns the output
		if( outputData["publicKey"] != blockData.pubKey ){
			return false;
		} 
		
		// check that the stake is calculated correctly
		const uint64_t age = block.getHeight() - heightLastStaked[blockData.outputId];
		if( age <= 0 ){
			return false;
		} 
		CryptoKernel::BigNum stakeConsumed = this->calculateStakeConsumed(age, outputValue);
		if( stakeConsumed != blockData.stakeConsumed ){
			return false;
		}
		
		// check that the target is calculated corrctly
		CryptoKernel::BigNum target = this->calculateTarget(transaction, block.getPreviousBlockId());
		if( target != blockData.target ){
			return false;
		}
	
		// verify the stake was selected according to the target
		CryptoKernel::BigNum selectionValue = this->selectionFunction(stakeConsumed, blockId, blockData.timestamp, blockData.outputId);
		if( selectionValue > target ){
			return false;	
		}

		const CryptoKernel::PoSNaive::ConsensusData prevBlockData = CryptoKernel::PoSNaive::getConsensusData(previousBlock);
		CryptoKernel::BigNum inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - target;
		// verify that the total work is updated correctly
		if( blockData.totalWork != ( inverse + prevBlockData.totalWork ) ){
			return false;
		}
		
		// verify that the total stake consumed is updated correctly
		if( blockData.totalStakeConsumed != ( stakeConsumed + prevBlockData.totalStakeConsumed ) ){
			return false;
		}

		// TODO - check timestamp here?
		// TODO - check signature here?
		
		return true;
	} catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
		return false;
	};		
};

bool CryptoKernel::PoSNaive::verifyTransaction(Storage::Transaction *transaction, 
			const CryptoKernel::Blockchain::transaction& tx){
	return true;
}

bool CryptoKernel::PoSNaive::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx){
	const std::set<CryptoKernel::Blockchain::input> inputs = tx.getInputs();
	const std::set<CryptoKernel::Blockchain::output> outputs = tx.getOutputs();
	CryptoKernel::Blockchain::block block = this->blockchain->getBlock(transaction, "tip");
	uint64_t height = block.getHeight()+1;
	
	// remove all outputids that have been consumed 
	for( const auto& input : inputs ) {
		CryptoKernel::BigNum outputId = input.getOutputId();
		heightLastStaked.erase(outputId.toString());	
	}

	// add all outputids that have been created, recording height
	for( const auto& output : outputs ) {
		CryptoKernel::BigNum outputId = output.getId();
		heightLastStaked[outputId.toString()] = height;
	}

	return true;
};

bool CryptoKernel::PoSNaive::submitTransaction(Storage::Transaction *transaction, const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::PoSNaive::submitBlock(Storage::Transaction *transaction, const CryptoKernel::Blockchain::block& block){
	CryptoKernel::PoSNaive::ConsensusData blockData = CryptoKernel::PoSNaive::getConsensusData(block);
	// update the height of the consumed output
	heightLastStaked[blockData.outputId] = block.getHeight();
	return true;	
};

void CryptoKernel::Consensus::start(){
};

CryptoKernel::PoSNaive::ConsensusData CryptoKernel::PoSNaive::getConsensusData(const CryptoKernel::Blockchain::block& block){
	CryptoKernel::PoSNaive::ConsensusData cd;
	const Json::Value cj = block.getConsensusData();
	try {
		cd.stakeConsumed = CryptoKernel::BigNum(cj["stakeConsumed"].asString());
		cd.target = CryptoKernel::BigNum(cj["target"].asString());
		cd.totalWork = CryptoKernel::BigNum(cj["totalWork"].asString());
		cd.totalStakeConsumed = CryptoKernel::BigNum(cj["totalStakeConsumed"].asString());
		cd.pubKey = cj["pubKey"].asString();
		cd.outputId = cj["outputId"].asString();
		cd.timestamp = cj["timestamp"].asUInt64();
		cd.signature = cj["signature"].asString();
	} catch(const Json::Exception& e) {
		throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
	}
	return cd;
};

CryptoKernel::PoSNaive::ConsensusData CryptoKernel::PoSNaive::getConsensusData(const CryptoKernel::Blockchain::dbBlock& block){
        CryptoKernel::PoSNaive::ConsensusData cd;                                              
        const Json::Value cj = block.getConsensusData(); 
        try { 
                cd.stakeConsumed = CryptoKernel::BigNum(cj["stakeConsumed"].asString());
		cd.target = CryptoKernel::BigNum(cj["target"].asString());
                cd.totalWork = CryptoKernel::BigNum(cj["totalWork"].asString());                        
		cd.totalStakeConsumed = CryptoKernel::BigNum(cj["totalStakeConsumed"].asString());
		cd.pubKey = cj["pubKey"].asString();
                cd.outputId = cj["outputId"].asString();
		cd.timestamp = cj["timestamp"].asUInt64();
		cd.signature = cj["signature"].asString();
	 } catch(const Json::Exception& e) {
                throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
        }       
        return cd;   	
};

Json::Value CryptoKernel::PoSNaive::consensusDataToJson(const CryptoKernel::PoSNaive::ConsensusData& cd){
	Json::Value consensusDataAsJson;
	consensusDataAsJson["stakeConsumed"] = cd.stakeConsumed.toString();
	consensusDataAsJson["target"] = cd.target.toString();
	consensusDataAsJson["totalWork"] = cd.totalWork.toString();
	consensusDataAsJson["totalStakeConsumed"] = cd.totalStakeConsumed.toString();
	consensusDataAsJson["pubKey"] = cd.pubKey;
	consensusDataAsJson["outputId"] = cd.outputId;
	consensusDataAsJson["timestamp"] = cd.timestamp;
	consensusDataAsJson["signature"] = cd.signature;
	return consensusDataAsJson;
};

CryptoKernel::BigNum CryptoKernel::PoSNaive::calculateStakeConsumed(
        const uint64_t age, const uint64_t amount){
	std::stringstream buffer;
	buffer << std::hex << age;
	CryptoKernel::BigNum bigAge = CryptoKernel::BigNum(buffer.str());
	std::stringstream buffer2;
	buffer2 << std::hex << amount;
	CryptoKernel::BigNum bigAmount = CryptoKernel::BigNum(buffer2.str()); 
	CryptoKernel::BigNum coinAge = bigAmount*this->amountWeight + bigAge*this->ageWeight;
	return coinAge;
};

CryptoKernel::BigNum CryptoKernel::PoSNaive::calculateTarget(Storage::Transaction* transaction, 
	const CryptoKernel::BigNum& previousBlockId){
	// PoW difficulty function unchanged
	const uint64_t minBlocks = 144;
	const uint64_t maxBlocks = 4032;
    	const CryptoKernel::BigNum minDifficulty =
        	CryptoKernel::BigNum("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
   	CryptoKernel::Blockchain::dbBlock currentBlock = blockchain->getBlockDB(transaction,
        	    previousBlockId.toString());
    	CryptoKernel::PoSNaive::ConsensusData currentBlockData = 
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

CryptoKernel::BigNum selectionFunction(const CryptoKernel::BigNum& stakeConsumed, const CryptoKernel::BigNum& blockId, const uint64_t timestamp, const std::string& outputId){
	std::stringstream buffer;
	buffer << blockId.toString() << timestamp << outputId;
	CryptoKernel::Crypto crypto;
	CryptoKernel::BigNum hash = CryptoKernel::BigNum(crypto.sha256(buffer.str()));
	CryptoKernel::BigNum finalValue = finalValue/stakeConsumed; 
	return finalValue;
};


