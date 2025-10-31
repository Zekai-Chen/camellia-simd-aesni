/*
 * Test with distinctive blocks to trace exact mapping
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
    uint8_t output_c[256];
    uint8_t output_asm[256];

    printf("========================================\n");
    printf("Distinctive Blocks Test\n");
    printf("========================================\n\n");

    // Create distinctive blocks: each block has pattern (block_num << 4) | byte_pos
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

    // Encrypt with C
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);

    printf("C output blocks (first 4):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output_c[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    // Encrypt with Assembly
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);

    printf("ASM output blocks (first 4):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output_asm[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    // Compare
    int match_count = 0;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] == output_asm[i]) {
            match_count++;
        }
    }

    printf("Match: %d/256 bytes\n", match_count);

    if (match_count == 256) {
        printf("✓ SUCCESS: Complete match!\n");
        return 0;
    } else {
        printf("✗ FAILED: %d bytes differ\n", 256 - match_count);

        // Show which blocks match
        printf("\nBlock-by-block comparison:\n");
        for (int block = 0; block < 16; block++) {
            int block_matches = 0;
            for (int byte = 0; byte < 16; byte++) {
                if (output_c[block * 16 + byte] == output_asm[block * 16 + byte]) {
                    block_matches++;
                }
            }
            printf("  Block %2d: %2d/16 bytes match\n", block, block_matches);
        }
        return 1;
    }
}
