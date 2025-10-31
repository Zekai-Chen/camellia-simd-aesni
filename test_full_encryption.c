/*
 * Full encryption comparison: C vs Assembly
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// Test key
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Test plaintext (16 identical blocks)
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t output_c[256];
    uint8_t output_asm[256];

    printf("========================================\n");
    printf("Full Encryption Comparison Test\n");
    printf("========================================\n\n");

    // Setup key
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    printf("Key setup successful\n");
    printf("  key_table[0] (kw1): 0x%016lx\n", ctx.key_table[0]);
    printf("  key_table[1] (kw2): 0x%016lx\n", ctx.key_table[1]);
    printf("  key_table[24] (lastk, post-whitening): 0x%016lx\n\n", ctx.key_table[24]);

    // Prepare 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    printf("Input (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Encrypt using C implementation
    printf("Encrypting with C implementation...\n");
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);

    printf("C output (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", output_c[i]);
    }
    printf("\n\n");

    // Encrypt using Assembly implementation
    printf("Encrypting with Assembly implementation...\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);

    printf("ASM output (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", output_asm[i]);
    }
    printf("\n\n");

    // Compare all 16 blocks
    printf("Comparison:\n");
    printf("-----------\n");
    int errors = 0;
    for (int block = 0; block < 16; block++) {
        int block_errors = 0;
        for (int byte = 0; byte < 16; byte++) {
            int idx = block * 16 + byte;
            if (output_c[idx] != output_asm[idx]) {
                block_errors++;
                errors++;
            }
        }
        if (block_errors > 0) {
            printf("Block %2d: %d bytes differ\n", block, block_errors);
        } else {
            printf("Block %2d: OK\n", block);
        }
    }

    printf("\n");
    if (errors == 0) {
        printf("✓ SUCCESS: Full encryption matches!\n");
        return 0;
    } else {
        printf("✗ FAILED: %d bytes differ\n", errors);

        // Show blocks 6-7 in detail since they're partially correct
        printf("\nBlocks 6-7 detailed comparison:\n");
        for (int block = 6; block < 8; block++) {
            printf("Block %d:\n", block);
            printf("  C:   ");
            for (int i = 0; i < 16; i++) {
                printf("%02x ", output_c[block * 16 + i]);
            }
            printf("\n  ASM: ");
            for (int i = 0; i < 16; i++) {
                printf("%02x ", output_asm[block * 16 + i]);
            }
            printf("\n  Matching bytes: ");
            for (int i = 0; i < 16; i++) {
                if (output_c[block * 16 + i] == output_asm[block * 16 + i]) {
                    printf("%2d ", i);
                }
            }
            printf("\n\n");
        }

        printf("First differing block (Block 0):\n");
        printf("  C:   ");
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output_c[i]);
        }
        printf("\n  ASM: ");
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output_asm[i]);
        }
        printf("\n");
        return 1;
    }
}
