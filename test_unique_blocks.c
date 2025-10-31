/*
 * Test with unique content for each block to trace mapping
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
    uint8_t input[256];
    uint8_t output_c[256];
    uint8_t output_asm[256];

    printf("========================================\n");
    printf("Unique Blocks Test\n");
    printf("========================================\n\n");

    // Each block has unique first byte = block number
    for (int block = 0; block < 16; block++) {
        input[block * 16] = block;  // Block identifier
        for (int i = 1; i < 16; i++) {
            input[block * 16 + i] = (block << 4) | i;
        }
    }

    printf("Input blocks (first byte of each):\n");
    for (int block = 0; block < 16; block++) {
        printf("  Block %2d: first_byte=0x%02x\n", block, input[block * 16]);
    }
    printf("\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);

    printf("C output (first 4 bytes of each block):\n");
    for (int block = 0; block < 16; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 4; i++) {
            printf("%02x", output_c[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    printf("ASM output (first 4 bytes of each block):\n");
    for (int block = 0; block < 16; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 4; i++) {
            printf("%02x", output_asm[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    // Find which ASM block matches each C block
    printf("Block mapping (which ASM block matches which C block):\n");
    for (int c_block = 0; c_block < 16; c_block++) {
        for (int asm_block = 0; asm_block < 16; asm_block++) {
            if (memcmp(&output_c[c_block * 16], &output_asm[asm_block * 16], 16) == 0) {
                printf("  C block %2d == ASM block %2d\n", c_block, asm_block);
                break;
            }
        }
    }

    return 0;
}
