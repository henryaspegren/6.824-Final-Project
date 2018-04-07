#include "PoSNaive.h"
#include "../crpyto.h"

CryptoKernel::Consensus::PoSNaive::PoSNaive(const uint64_t amountWeight, const uint64_t ageWeight, std::string pubKey){
	this->amountWeight = amountWeight;
	this->ageWeight = ageWeight;
	this->pubKey = pubKey;
}

bool CryptoKernel::Consensus::PoSNaive::isBlockBetter(Storage::Transaction* transaction,
	const CryptoKernel::Blockchain::block& block, 
	const CryptoKernel::Blockchain::dbBlock& tip){
	
	const CryptoKernel::Consensus::PoSNaive::ConsensusData blockData = getConsensusData(block);
	const ConsensusData::Consensus::PoSNaive::ConsensusData tipData = getConsensusData(tip);
	
	return  blockData.totalStakeConsumed > tipData.totalStakeConsumed;
};

bool CryptoKernel::Consensus::PoSNaive::checkConsensusRule(Storage::Transaction* transaction, 
	const CryptoKernel::Blockchain::block& block,
	const CryptoKernel::Blockchain::dbBlock& previousBlock){
	
	try{ 
		const CryptoKernel::Consensus::PoSNaive::ConsensusData blockData = CryptoKernel::Consensus:PoSNaive::getConsensusData(block); 
		
		const CryptoKernel::Blockchain::output output = blockchain->getOutput(blockData.outputId);
		const uint64_t outputValue = output.getValue();
		const Json::Value outputData = output.getData();
		
		// check that the winning pubkey owns the output
		if( outputData["publicKey"] != blockData.pubKey ){
			return false;
		} 
		
		// check that the stake is calculated correctly
		const uint64_t age = block.getHeight() - heightLastStaked[blockData.outputId];
		if( age <= 0 ){
			return false;
		} 
		CryptoKernel::BigNum stakeConsumed = CryptoKernel::Consensus::PoSNaive::calculateStakeConsumed(age, outputValue);
		if( stakeConsumed != blockData.stakeConsumed ){
			return false;
		}
		
		// check that the target is calculated corrctly
		CryptoKernel::BigNum target = CryptoKernel::Consensus::PoSNaive::calculateTarget(transaction, block.getPreviousBlockId(), stakeConsumed);
		if( target != blockData.target ){
			return false;
		}
	
		// verify the stake was selected according to the target
		CryptoKernel::BigNum selectionValue = CryptoKernel::Consensus::PoSNaive::selectionFunction, block.getId(), blockData.timestamp, blockData.outputId);
		if( selectionValue > target ){
			return false;	
		}

		const CryptoKernel::Consensus::PoSNaive::ConsensusData prevBlockData = CryptoKernel::Consensus::PoSNaive::getConsensusData(previousBlock);

		// verify that the total work is updated correctly
		if( blockData.totalWork != ( target + prevBlockData.totalWork ) ){
			return false;
		}
		
		// verify that the total stake consumed is updated correctly
		if( blockData.totalStakeConsumed != ( stakeConsumed + prevBlockData.totalStakeConsumed ) ){
			return false;
		}

		// TODO - check timestamp here?
		
		return true

	} catch(const CryptoKernel::Blockchain::InvalidElementException& e) {
		return false;
	}		
};

bool verifyTransaction(Storage::Transaction *transaction, 
			const CryptoKernel::BigNum& previousBlockId,
			const std::string& publicKey){
	return true;
}

bool CryptoKernel::Consensus::PoSNaive::confirmTransaction(Storage::Transaction* transaction, const CryptoKernel::Blockchain::transaction& tx){
	const std::set<CryptoKernel::Blockchain::input> inputs = tx.getInputs();
	const std::set<CryptoKernel::Blockchain::output> outputs = tx.getOutputs();i
	//TODO - how to safely get the height of the block that includes
	// these transactions? 
	uint64_t height = 0;
	
	// remove all outputids that have been consumed 
	for( const auto& input : inputs ) {
		CryptoKernel::BigNum outputId = input.getOutputId();
		heightLastStaked.erase(outputId.toString());	
	}

	// add all outputids that have been created, recording height
	for( const auto& output : outputs ) {
		CryptoKernel::BigNum outputId = output.getNonce();
		heightLastStaked[outputId.toString()] = height;
	}

	return true;
};

bool CrpytoKernel::Consensus::PoSNaive::submitTransaction(Storage::Transaction *transaction, const CryptoKernel::Blockchain::transaction& tx){
	return true;
};

bool CryptoKernel::Consensus::PoSNaive::submitBlock(Storage::Transaction *transaction, const CryptoKernel::Blockchain::block& block){
	CryptoKernel::Consensus::PoSNaive::ConsensusData blockData = CryptoKernel::Consensus::PoSNaive::getConsensusData(block);
	// update the height of the consumed output
	heightLastStaked[blockData.pubKey] = block.getHeight();
	return true;	
};

void CryptoKernel::Consensus::start(){
};

CryptoKernel::Consensus::PoSNaive::ConsensusData CrpytoKernel::Consensus::PoSNaive::getConsensusData(const CryptoKernel::Blockchain::block& block){

};

CryptoKernel::Consensus::PoSNaive::ConsensusData CryptoKernel::Consensus::PoSNaive::getConsensusData(const CryptoKernel::Blockchain::dbBlock& dbBlock){

};

Json::Value CryptoKernel::PoSNaive::consensusDataToJson(const CrypotKernel::Consensus::PoSNaive::ConsensusData& cd){
	
};

CryptoKernel::BigNum CrytoKernel::Consensus::PoSNaive::calculateStakeConsumed(
        const uint64_t age, const uint64_t amount){
	// TODO how to do this with BigNums..
	const uint64_t coinAge = amountWeight*amount + ageWeight*age;
	return CryptoKernel::BigNum(coinAge);
};

CryptoKernel::BigNum CrpytoKernel::Consensus::PoSNaive::calculateTarget(Storage::Transaction* transaction, 
	const CryptoKernel::BigNum& prevBlockId){
	// TODO - use some other difficulty calculation (e.g. PoWs)
	return CryptoKernel::BigNum("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
}

CryptoKernel::BigNum selectionFunction(const CryptoKernel::BigNum& stakeConsumed, const std::string& blockId, const std::string& timestamp, const std::string& outputId){
	// TODO - SHA_256(blockId||timestamp) somehow scaled by stakeConsumed
	std::stringstream buffer;
	buffer << blockId << timestamp << outputId;
	CryptoKernel::BigNum hash = CryptoKernel::BigNum(crypto.sha256(buffer.str() ))	
	return hash-stakeConsumed;
};


