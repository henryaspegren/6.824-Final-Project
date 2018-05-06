#include <string>
#include <iostream>

#include <cschnorr/key.h>

#include "../src/kernel/base64.h"

int main() {
    schnorr_context* ctx = schnorr_context_new();
    
    // Generate new R point
	committed_r_key* newKey = committed_r_key_new(ctx);

    unsigned char* newk = new unsigned char[32];
	if(BN_bn2binpad(newKey->k, newk, 32) != 32) {
		throw std::runtime_error("Failure encoding k value");
	}

    std::cout << "k: \n" << base64_encode(newk, 32) << "\n\n";
    
    std::cout << "r: \n" << base64_encode(newKey->pub->r, 32) << "\n\n";
    
    delete[] newk;
    committed_r_key_free(newKey);
    schnorr_context_free(ctx);
    
    return 0;
}
