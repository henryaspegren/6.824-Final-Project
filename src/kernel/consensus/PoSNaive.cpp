#include <sstream>
#include <math.h>

#include "PoSNaive.h"
#include "../crypto.h"

CryptoKernel::Consensus::PoSNaive::PoSNaive(const uint64_t blockTarget,
	CryptoKernel::Blockchain* blockchain, const bool run_miner, 
	const CryptoKernel::BigNum& amountWeight, 
	const CryptoKernel::BigNum& ageWeight, const std::string& pubKey){
	this->blockTarget = blockTarget;
	this->blockchain = blockchain;
	this->run_miner = run_miner;
	this->amountWeight = amountWeight;
	this->ageWeight = ageWeight;
	this->pubKey = pubKey;
};

CryptoKernel::Consensus::PoSNaive::~PoSNaive(){
	this->run_miner = false;
	this->minerThread->join();
};

bool CryptoKernel::Consensus::PoSNaive::isBlockBetter(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::block& block, 
	const CryptoKernel::Blockchain::dbBlock& tip){
	const CryptoKernel::Consensus::PoSNaive::ConsensusData blockData = this->getConsensusData(block);
	const CryptoKernel::Consensus::PoSNaive::ConsensusData tipData = this->getConsensusData(tip);	
	return  blockData.totalWork > tipData.totalWork;
};

bool CryptoKernel::Consensus::PoSNaive::checkConsensusRules(Storage::Transaction* transaction, 
	CryptoKernel::Blockchain::block& block,
	const CryptoKernel::Blockchain::dbBlock& previousBlock){
	try{ 
		const CryptoKernel::Consensus::PoSNaive::ConsensusData blockData = CryptoKernel::Consensus::PoSNaive::getConsensusData(block); 
		const CryptoKernel::Blockchain::output output = this->blockchain->getOutput(transaction, blockData.outputId);
		const uint64_t outputValue = output.getValue();
		const Json::Value outputData = output.getData();
		const uint64_t outputHeightLastStaked  = this->heightLastStaked[blockData.outputId];
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

		// check that the output height last staked and value are correct
		if( outputValue != blockData.outputValue || outputHeightLastStaked != blockData.outputHeightLastStaked ) {
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
		CryptoKernel::BigNum hash = this->calculateHash(blockId, blockData.timestamp, blockData.outputId);
		if( hash > target * stakeConsumed){
			return false;	
		}

		const CryptoKernel::Consensus::PoSNaive::ConsensusData prevBlockData = CryptoKernel::Consensus::PoSNaive::getConsensusData(previousBlock);
		CryptoKernel::BigNum inverse = CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - target;
		// verify that the total work is updated correctly
		if( blockData.totalWork != ( inverse + prevBlockData.totalWork ) ){
			return false;
		}

		// TODO - check timestamp here?
		// TODO - check signature here?
		
		return true;
	} catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
		return false;
	};		
};

Json::Value CryptoKernel::Consensus::PoSNaive::generateConsensusData(Storage::Transaction* transaction,
	const CryptoKernel::BigNum& previousBlockId,
	const std::string& publicKey){
	CryptoKernel::Consensus::PoSNaive::ConsensusData cd;
	cd.target = CryptoKernel::Consensus::PoSNaive::calculateTarget(transaction, previousBlockId);
	Json::Value asJson = CryptoKernel::Consensus::PoSNaive::consensusDataToJson(cd);
	return asJson;
};

void CryptoKernel::Consensus::PoSNaive::miner(){
	time_t t = std::time(0);
	uint64_t now = static_cast<uint64_t> (t);
	while (run_miner) {
		CryptoKernel::Blockchain::block block = this->blockchain->generateVerifyingBlock(pubKey);
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
		CryptoKernel::BigNum target = CryptoKernel::BigNum(consensusDataThisBlock["target"].asString());
		bool blockMined = false;
		CryptoKernel::BigNum previousBlockId = block.getPreviousBlockId();
		do{
			t = std::time(0);
			time2 = static_cast<uint64_t>(t);
			
			block.setTimestamp(time2);
			
			if((time2-now) % 20 == 0 && (time2 - now) > 0){
				// update block we are 'mining' on top of 
				auto newBlock = this->blockchain->generateVerifyingBlock(pubKey);
				if(newBlock.getPreviousBlockId() != previousBlockId) {
				    previousBlockId = block.getPreviousBlockId();
				    previousBlock = this->blockchain->getBlockDB(
					    block.getPreviousBlockId().toString());
                    		    height = block.getHeight();
                                    blockId = block.getId();
				    consensusDataThisBlock = block.getConsensusData();
				    consensusDataPreviousBlock = previousBlock.getConsensusData();
				    totalWorkPrev = CryptoKernel::BigNum(
                            		consensusDataPreviousBlock["totalWork"].asString());
				    target = CryptoKernel::BigNum(consensusDataThisBlock["target"].asString());
				    now = time2;
				}
			}
			// check to see if any of our staked outputs are selected
			for( auto const& entry :  stakedOutputValues ){
				// only check the outputs that can be staked
				std::string outputId = entry.first;
				if( !this->canBeStaked[outputId] ){
					continue;
				}

				uint64_t value = entry.second;
				uint64_t outputHeightLastStaked = this->heightLastStaked[outputId];
				uint64_t age = height - outputHeightLastStaked;
				CryptoKernel::BigNum stakeConsumed = 
					this->calculateStakeConsumed(age, value);
				CryptoKernel::BigNum hash = 
					this->calculateHash(blockId, time2, outputId);
				// output selected
				if( hash < target * stakeConsumed) { 
				        consensusDataThisBlock["stakeConsumed"] = stakeConsumed.toString();
                                        consensusDataThisBlock["target"] = (target).toString();
                                        consensusDataThisBlock["totalWork"] = (CryptoKernel::BigNum("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") - (target) + totalWorkPrev).toString();
                                        consensusDataThisBlock["pubKey"] = pubKey;
                                        consensusDataThisBlock["outputId"] = outputId;
                                        consensusDataThisBlock["outputHeightLastStaked"] = outputHeightLastStaked;
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


bool CryptoKernel::Consensus::PoSNaive::verifyTransaction(Storage::Transaction *transaction, 
			const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::Consensus::PoSNaive::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx){
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
			this->stakedOutputValues[outputId.toString()] = output.getValue();
		}
	}

	return true;
};

bool CryptoKernel::Consensus::PoSNaive::submitTransaction(Storage::Transaction *transaction, const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::Consensus::PoSNaive::submitBlock(Storage::Transaction *transaction, const CryptoKernel::Blockchain::block& block){
	CryptoKernel::Consensus::PoSNaive::ConsensusData blockData = CryptoKernel::Consensus::PoSNaive::getConsensusData(block);
	// update the height of the consumed output
	this->heightLastStaked[blockData.outputId] = block.getHeight();
	return true;	
};

void CryptoKernel::Consensus::PoSNaive::reverseBlock(Storage::Transaction *transaction){
	const CryptoKernel::Blockchain::block& tip = this->blockchain->getBlock(transaction, "tip");
	const Json::Value consensusDataJson = tip.getConsensusData();
	// first "unstake" the output that stakes this block
	this->heightLastStaked[consensusDataJson["outputId"].asString()] = consensusDataJson["outputHeightLastStaked"].asUInt64();
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

void CryptoKernel::Consensus::PoSNaive::start(){
	this->minerThread.reset(new std::thread(&CryptoKernel::Consensus::PoSNaive::miner, this));
};

CryptoKernel::Consensus::PoSNaive::ConsensusData CryptoKernel::Consensus::PoSNaive::getConsensusData(const CryptoKernel::Blockchain::block& block){
	CryptoKernel::Consensus::PoSNaive::ConsensusData cd;
	const Json::Value cj = block.getConsensusData();
	try {
		cd.stakeConsumed = CryptoKernel::BigNum(cj["stakeConsumed"].asString());
		cd.target = CryptoKernel::BigNum(cj["target"].asString());
		cd.totalWork = CryptoKernel::BigNum(cj["totalWork"].asString());
		cd.pubKey = cj["pubKey"].asString();
		cd.outputId = cj["outputId"].asString();
		cd.outputHeightLastStaked  = cj["outputHeightLastStaked"].asUInt64();
		cd.outputValue = cj["outputValue"].asUInt64();
		cd.timestamp = cj["timestamp"].asUInt64();
		cd.signature = cj["signature"].asString();
	} catch(const Json::Exception& e) {
		throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
	}
	return cd;
};

CryptoKernel::Consensus::PoSNaive::ConsensusData CryptoKernel::Consensus::PoSNaive::getConsensusData(const CryptoKernel::Blockchain::dbBlock& block){
        CryptoKernel::Consensus::PoSNaive::ConsensusData cd;                                              
        const Json::Value cj = block.getConsensusData(); 
        try { 
                cd.stakeConsumed = CryptoKernel::BigNum(cj["stakeConsumed"].asString());
		cd.target = CryptoKernel::BigNum(cj["target"].asString());
                cd.totalWork = CryptoKernel::BigNum(cj["totalWork"].asString());                        
		cd.pubKey = cj["pubKey"].asString();
                cd.outputId = cj["outputId"].asString();
		cd.outputHeightLastStaked = cj["outputHeightLastStaked"].asUInt64();
		cd.outputValue = cj["outputValue"].asUInt64();
		cd.timestamp = cj["timestamp"].asUInt64();
		cd.signature = cj["signature"].asString();
	 } catch(const Json::Exception& e) {
                throw CryptoKernel::Blockchain::InvalidElementException("Block consensusData JSON is malformed");
        }       
        return cd;   	
};

Json::Value CryptoKernel::Consensus::PoSNaive::consensusDataToJson(const CryptoKernel::Consensus::PoSNaive::ConsensusData& cd){
	Json::Value consensusDataAsJson;
	consensusDataAsJson["stakeConsumed"] = cd.stakeConsumed.toString();
	consensusDataAsJson["target"] = cd.target.toString();
	consensusDataAsJson["totalWork"] = cd.totalWork.toString();
	consensusDataAsJson["pubKey"] = cd.pubKey;
	consensusDataAsJson["outputId"] = cd.outputId;
	consensusDataAsJson["outputHeightLastStaked"] = cd.outputHeightLastStaked;
	consensusDataAsJson["outputValue"] = cd.outputValue; 
	consensusDataAsJson["timestamp"] = cd.timestamp;
	consensusDataAsJson["signature"] = cd.signature;
	return consensusDataAsJson;
};

CryptoKernel::BigNum CryptoKernel::Consensus::PoSNaive::calculateStakeConsumed(
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

CryptoKernel::BigNum CryptoKernel::Consensus::PoSNaive::calculateTarget(Storage::Transaction* transaction, 
	const CryptoKernel::BigNum& previousBlockId){
	// PoW difficulty function unchanged
	const uint64_t minBlocks = 144;
	const uint64_t maxBlocks = 4032;
    	const CryptoKernel::BigNum minDifficulty =
        	CryptoKernel::BigNum("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
   	CryptoKernel::Blockchain::dbBlock currentBlock = blockchain->getBlockDB(transaction,
        	    previousBlockId.toString());
    	CryptoKernel::Consensus::PoSNaive::ConsensusData currentBlockData = 
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

CryptoKernel::BigNum CryptoKernel::Consensus::PoSNaive::calculateHash(const CryptoKernel::BigNum& blockId, const uint64_t timestamp, const std::string& outputId){
	std::stringstream buffer;
	buffer << blockId.toString() << timestamp << outputId;
	CryptoKernel::Crypto crypto;
	return CryptoKernel::BigNum(crypto.sha256(buffer.str()));
};


