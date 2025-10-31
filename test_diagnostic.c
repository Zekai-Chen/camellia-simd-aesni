/*
 * Diagnostic test - compare first two blocks only
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern void camellia_encrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
                                             void *out, const void *in);
extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                          void *out, const void *in);

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output_c[256], output_asm[256];

    printf("========================================\n");
    printf("Diagnostic Test - First Two Blocks\n");
    printf("========================================\n\n");

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Prepare input (all blocks identical)
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    // Encrypt with C
    printf("Step 1: C implementation\n");
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_c[i]);
    printf("\n");
    printf("  Block 1: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_c[16 + i]);
    printf("\n\n");

    // Encrypt with Assembly
    printf("Step 2: Assembly implementation\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_asm[i]);
    printf("\n");
    printf("  Block 1: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_asm[16 + i]);
    printf("\n\n");

    // Compare
    int errors = 0;
    for (int i = 0; i < 32; i++) {
        if (output_c[i] != output_asm[i]) {
            if (errors < 16) {
                printf("  Diff[%2d]: C=%02x ASM=%02x\n", i, output_c[i], output_asm[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("✓ First two blocks match!\n");
        return 0;
    } else {
        printf("✗ %d/%d bytes differ in first two blocks\n", errors, 32);
        return 1;
    }
}
