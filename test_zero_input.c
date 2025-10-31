/*
 * Test with all-zero input to simplify debugging
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                        void *out, const void *in);

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input_zeros[256] = {0};  // All zeros
    uint8_t output_c[256];
    uint8_t output_asm[256];

    printf("========================================\n");
    printf("Test with All-Zero Input\n");
    printf("========================================\n\n");

    /* Setup key */
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    /* Encrypt all-zero blocks with C */
    camellia_encrypt_16blks_simd128(&ctx, output_c, input_zeros);

    /* Encrypt all-zero blocks with ASM */
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input_zeros);

    /* Compare block 0 (should be same since input is same) */
    printf("Block 0 comparison (all-zero input):\n");
    printf("C:   ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", output_c[i]);
    }
    printf("\n");

    printf("ASM: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", output_asm[i]);
    }
    printf("\n");

    if (memcmp(output_c, output_asm, 16) == 0) {
        printf("✓ Block 0 MATCHES!\n\n");
    } else {
        printf("✗ Block 0 DIFFERS\n\n");

        /* Show differences */
        printf("Differences in block 0:\n");
        for (int i = 0; i < 16; i++) {
            if (output_c[i] != output_asm[i]) {
                printf("  Byte %2d: C=%02x ASM=%02x\n", i, output_c[i], output_asm[i]);
            }
        }
        printf("\n");
    }

    /* Check all blocks for patterns */
    printf("All 16 blocks comparison:\n");
    int total_match = 0;
    for (int block = 0; block < 16; block++) {
        int match = memcmp(&output_c[block * 16], &output_asm[block * 16], 16) == 0;
        if (match) total_match++;
        printf("Block %2d: %s\n", block, match ? "✓ MATCH" : "✗ DIFFER");
    }

    printf("\nTotal: %d/16 blocks match\n", total_match);

    /* Show first few bytes of each block to check for patterns */
    printf("\nASM output pattern (first 8 bytes of each block):\n");
    for (int block = 0; block < 16; block++) {
        printf("Block %2d: ", block);
        for (int i = 0; i < 8; i++) {
            printf("%02x", output_asm[block * 16 + i]);
        }
        printf("\n");
    }

    return 0;
}
