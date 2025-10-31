/*
 * Test C implementation: inpack16_pre and inpack16_post (byteslice)
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
    uint8_t output_full[256];

    printf("========================================\n");
    printf("Test C: inpack16_pre/post (byteslice)\n");
    printf("========================================\n\n");

    // Create distinctive input blocks
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    printf("Input blocks (first 4):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", input[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("Pre-whitening key (key_table[0]): 0x%016lx\n\n", ctx.key_table[0]);

    // Test: do full encryption then analyze
    camellia_encrypt_16blks_simd128(&ctx, output_full, input);

    printf("Full encryption output (first 4 blocks):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output_full[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("✓ This establishes baseline for C implementation\n");
    printf("Next: Test individual components (inpack, rounds, etc)\n");

    return 0;
}
