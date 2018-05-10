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

    unsigned char* newR = new unsigned char[33];
    if(EC_POINT_point2oct(ctx->group, newKey->pub->R, POINT_CONVERSION_COMPRESSED, newR, 33, ctx->bn_ctx) != 33) {
        throw std::runtime_error("Failure encoding R point");
    }  

    std::cout << "k: \n" << base64_encode(newk, 32) << "\n\n";
    
    std::cout << "r: \n" << base64_encode(newR, 33) << "\n\n";
    
    delete[] newk;
    delete[] newR;
    committed_r_key_free(newKey);
    schnorr_context_free(ctx);
    
    return 0;
}
