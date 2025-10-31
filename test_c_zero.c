/*
 * Check C implementation with all-zero input
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input_zeros[256] = {0};
    uint8_t output_c[256];

    printf("C implementation with all-zero input:\n");
    printf("====================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    camellia_encrypt_16blks_simd128(&ctx, output_c, input_zeros);

    printf("All 16 blocks output:\n");
    for (int block = 0; block < 16; block++) {
        printf("Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x", output_c[block * 16 + i]);
        }
        printf("\n");
    }

    /* Check if all blocks are the same */
    int all_same = 1;
    for (int block = 1; block < 16; block++) {
        if (memcmp(&output_c[0], &output_c[block * 16], 16) != 0) {
            all_same = 0;
            break;
        }
    }

    printf("\n%s All blocks are %s\n",
           all_same ? "✓" : "✗",
           all_same ? "IDENTICAL (expected for all-zero input)" : "DIFFERENT");

    return 0;
}
