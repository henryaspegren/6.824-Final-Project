#include <sstream>
#include <math.h>

#include "PoSNaive.h"
#include "../crypto.h"

CryptoKernel::PoSNaive::PoSNaive(const uint64_t blockTarget,
	CryptoKernel::Blockchain* blockchain, const bool run_miner, 
	const CryptoKernel::BigNum amountWeight, 
	const CryptoKernel::BigNum ageWeight, std::string pubKey){
	this->blockTarget = blockTarget;
	this->blockchain = blockchain;
	this->run_miner = run_miner;
	this->amountWeight = amountWeight;
	this->ageWeight = ageWeight;
	this->pubKey = pubKey;
};

CryptoKernel::PoSNaive::~PoSNaive(){
	this->run_miner = false;
	this->minerThread->join();
};

bool CryptoKernel::PoSNaive::isBlockBetter(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::block& block, 
	const CryptoKernel::Blockchain::dbBlock& tip){
	const CryptoKernel::PoSNaive::ConsensusData blockData = this->getConsensusData(block);
	const CryptoKernel::PoSNaive::ConsensusData tipData = this->getConsensusData(tip);	
	return  blockData.totalStakeConsumed > tipData.totalStakeConsumed;
};

bool CryptoKernel::PoSNaive::checkConsensusRules(Storage::Transaction* transaction, 
	CryptoKernel::Blockchain::block& block,
	const CryptoKernel::Blockchain::dbBlock& previousBlock){
	try{ 
		const CryptoKernel::PoSNaive::ConsensusData blockData = CryptoKernel::PoSNaive::getConsensusData(block); 
		const CryptoKernel::Blockchain::output output = this->blockchain->getOutput(blockData.outputId);
		const uint64_t outputValue = output.getValue();
		const Json::Value outputData = output.getData();
		const uint64_t outputAge = this->heightLastStaked[blockData.outputId];
		const bool outputCanBeStaked = this->canBeStaked[blockData.outputId];
		const CryptoKernel::BigNum blockId = block.getId();
	
		// check that the output can be staked
		if( !outputCanBeStaked ){
			return false;
		}

		// check that the winning pubkey owns the output
		if( outputData["publicKey"].asString() != blockData.pubKey ){
			return false;
		}

		// check that the output age and value are correct
		if( outputValue != blockData.outputValue || outputAge != blockData.outputAge ) {
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
		if( selectionValue < target ){
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

Json::Value CryptoKernel::PoSNaive::generateConsensusData(Storage::Transaction* transaction,
	const CryptoKernel::BigNum& previousBlockId,
	const std::string& publicKey){
	CryptoKernel::PoSNaive::ConsensusData cd;
	cd.target = CryptoKernel::PoSNaive::calculateTarget(transaction, previousBlockId);
	Json::Value asJson = CryptoKernel::PoSNaive::consensusDataToJson(cd);
	return asJson;
};

void CryptoKernel::PoSNaive::miner(){
	time_t t = std::time(0);
	uint64_t now = static_cast<uint64_t> (t);
	while (run_miner) {
		CryptoKernel::Blockchain::block block = blockchain->generateVerifyingBlock(pubKey);
		CryptoKernel::Blockchain::dbBlock previousBlock = this->blockchain->getBlockDB(
                        block.getPreviousBlockId().toString());
		uint64_t height = block.getHeight();
                CryptoKernel::BigNum blockId = block.getId();

		t = std::time(0);
		now = static_cast<uint64_t> (t);
		uint64_t time2 = now;

		Json::Value consensusDataThisBlock = block.getConsensusData();
		Json::Value consensusDataPreviousBlock = previousBlock.getConsensusData();	
		CryptoKernel::BigNum totalWorkPrev = CryptoKernel::BigNum(
			consensusDataPreviousBlock["totalWork"].asString());
		CryptoKernel::BigNum totalStakeConsumed = CryptoKernel::BigNum(
			consensusDataPreviousBlock["totalStakeConsumed"].asString());		
		CryptoKernel::BigNum target = CryptoKernel::BigNum(consensusDataThisBlock["target"].asString());
		CryptoKernel::BigNum inverse = 
			CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - target;
		bool blockMined = false;
		do{
			t = std::time(0);
			time2 = static_cast<uint64_t>(t);
			if((time2-now) % 20 == 0 && (time2 - now) > 0){
				// update block we are 'mining' on top of 
				block = blockchain->generateVerifyingBlock(pubKey);
                 		previousBlock = this->blockchain->getBlockDB(
					block.getPreviousBlockId().toString());
                		height = block.getHeight();
                		blockId = block.getId();
				consensusDataThisBlock = block.getConsensusData();
				consensusDataPreviousBlock = previousBlock.getConsensusData();
				totalWorkPrev = CryptoKernel::BigNum(
                        		consensusDataPreviousBlock["totalWork"].asString());
				totalStakeConsumed = CryptoKernel::BigNum(
                        		consensusDataPreviousBlock["totalStakeConsumed"].asString()); 
				target = CryptoKernel::BigNum(consensusDataThisBlock["target"].asString());
				inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") 	- target;
				now = time2;	
			}
			// check to see if any of our staked outputs are selected
			for( auto const& entry :  stakedOutputValues ){
				// only check the ones that can be staked
				std::string outputId = entry.first;
				if( !this->canBeStaked[outputId] ){
					continue;
				}

				uint64_t value = entry.second;
				uint64_t age = height - this->heightLastStaked[outputId];
				CryptoKernel::BigNum stakeConsumed = 
					this->calculateStakeConsumed(age, value);
				CryptoKernel::BigNum selectionValue = 
					this->selectionFunction(stakeConsumed, 
						blockId, time2, outputId);
				// output selected
				if( selectionValue < target ) { 
				        consensusDataThisBlock["stakeConsumed"] = stakeConsumed.toString();
                                        consensusDataThisBlock["target"] = target.toString();
                                        consensusDataThisBlock["totalWork"] = (inverse + totalWorkPrev).toString();
                                        consensusDataThisBlock["totalStakeConsumed"] = (totalStakeConsumed + stakeConsumed).toString();
                                        consensusDataThisBlock["pubKey"] = pubKey;
                                        consensusDataThisBlock["outputId"] = outputId;
                                        consensusDataThisBlock["outputAge"] = age;
					consensusDataThisBlock["outputValue"] = value;
					consensusDataThisBlock["timestamp"] = time2;
                                        // TODO - sign here
                                        consensusDataThisBlock["signature"] = "";
					block.setConsensusData(consensusDataThisBlock);
					this->blockchain->submitBlock(block);
					blockMined = true;
				}
			}
		} while( run_miner && !blockMined );
	};
};


bool CryptoKernel::PoSNaive::verifyTransaction(Storage::Transaction *transaction, 
			const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::PoSNaive::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx){
	const std::set<CryptoKernel::Blockchain::input> inputs = tx.getInputs();
	const std::set<CryptoKernel::Blockchain::output> outputs = tx.getOutputs();
	CryptoKernel::Blockchain::block block = this->blockchain->getBlock(transaction, "tip");
	uint64_t height = block.getHeight()+1;
	
	// mark all outputids so they cannot be staked again
	// BUT keep the heightLastStaked incase a reorg requires
	// us to re-stake them  
	for( const auto& input : inputs ) {
		CryptoKernel::BigNum outputId = input.getOutputId();
		this->canBeStaked[outputId.toString()] = false;
	}

	// add all outputids that have been created, recording height
	for( const auto& output : outputs ) {
		CryptoKernel::BigNum outputId = output.getId();
		Json::Value data = output.getData();
		std::string outputPubKey = data["publicKey"].asString();
		this->heightLastStaked[outputId.toString()] = height;
		this->canBeStaked[outputId.toString()] = true;
		// if this the pubKey we are mining for, add it to the stake pool
		if( pubKey ==  outputPubKey ){
			stakedOutputValues[outputId.toString()] = output.getValue();
		}
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

void CryptoKernel::PoSNaive::reverseBlock(Storage::Transaction *transaction){
	const CryptoKernel::Blockchain::block& tip = this->blockchain->getBlock(transaction, "tip");
	const Json::Value consensusDataJson = tip.getConsensusData();
	// first "unstake" the output that stakes this block
	this->heightLastStaked[consensusDataJson["outputId"].asString()] = consensusDataJson["outputAge"].asUInt64();
	std::set<CryptoKernel::Blockchain::transaction> transactions = tip.getTransactions();
	for( const auto& transaction : transactions ) {
		std::set<CryptoKernel::Blockchain::input> inputs = transaction.getInputs();
		std::set<CryptoKernel::Blockchain::output> outputs = transaction.getOutputs();
		// since the outputs are no longer spent
		// mark them as stakeable
		for( const auto& input : inputs ) {
			std::string outputId = input.getOutputId().toString();
			this->canBeStaked[outputId] = true;
		}
		// newly created outputs no longer exist
		// so remove them entirely
		for( const auto& output : outputs ){
			std::string outputId = output.getId().toString();
			this->heightLastStaked.erase(outputId);
			this->canBeStaked.erase(outputId);
			this->stakedOutputValues.erase(outputId);
		}
	}	
			
};

void CryptoKernel::PoSNaive::start(){
	this->minerThread.reset(new std::thread(&CryptoKernel::PoSNaive::miner, this));
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
		cd.outputAge = cj["outputAge"].asUInt64();
		cd.outputValue = cj["outputValue"].asUInt64();
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
		cd.outputAge = cj["outputAge"].asUInt64();
		cd.outputValue = cj["outputValue"].asUInt64();
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
	consensusDataAsJson["outputAge"] = cd.outputAge;
	consensusDataAsJson["outputValue"] = cd.outputValue; 
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

CryptoKernel::BigNum CryptoKernel::PoSNaive::selectionFunction(const CryptoKernel::BigNum& stakeConsumed, const CryptoKernel::BigNum& blockId, const uint64_t timestamp, const std::string& outputId){
	std::stringstream buffer;
	buffer << blockId.toString() << timestamp << outputId;
	CryptoKernel::Crypto crypto;
	CryptoKernel::BigNum hash = CryptoKernel::BigNum(crypto.sha256(buffer.str()));
	CryptoKernel::BigNum finalValue = finalValue/stakeConsumed; 
	return finalValue;
};


