/*
 * Test C code's byteslice round-trip to understand exact behavior
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t output[256];
    
    // Create distinctive blocks
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }
    
    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    
    // Run full encryption
    camellia_encrypt_16blks_simd128(&ctx, output, input);
    
    printf("C Code Full Encryption Test (with distinctive blocks)\n");
    printf("======================================================\n\n");
    
    printf("Input blocks (first 4):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", input[block * 16 + i]);
        }
        printf("\n");
    }
    
    printf("\nOutput blocks (first 4):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output[block * 16 + i]);
        }
        printf("\n");
    }
    
    printf("\nThis shows C code's correct transformation.\n");
    printf("Now compare with Assembly output...\n");
    
    return 0;
}
