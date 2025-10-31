/*
 * Hex dump comparison - C vs Assembly at key points
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

void hex_dump(const char *label, const uint8_t *data, int len) {
    printf("%s:\n", label);
    for (int i = 0; i < len; i += 16) {
        printf("  %04x:", i);
        for (int j = 0; j < 16 && i + j < len; j++) {
            if (j % 4 == 0) printf(" ");
            printf("%02x", data[i + j]);
        }
        printf("\n");
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output_c[256], output_asm[256];

    printf("========================================\n");
    printf("Hex Dump Comparison\n");
    printf("========================================\n\n");

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Prepare input (all blocks identical)
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    // Encrypt with C
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);

    // Encrypt with Assembly
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);

    printf("C Output (first 64 bytes):\n");
    hex_dump("", output_c, 64);

    printf("\nAssembly Output (first 64 bytes):\n");
    hex_dump("", output_asm, 64);

    printf("\nDifferences (first 64 bytes):\n");
    int diff_count = 0;
    for (int i = 0; i < 64; i++) {
        if (output_c[i] != output_asm[i]) {
            if (diff_count % 16 == 0) {
                if (diff_count > 0) printf("\n");
                printf("  %04x:", i);
            }
            if (diff_count % 4 == 0 && diff_count % 16 != 0) printf(" ");
            printf(" C:%02x/A:%02x", output_c[i], output_asm[i]);
            diff_count++;
        }
    }
    printf("\n");

    if (memcmp(output_c, output_asm, 256) == 0) {
        printf("\n✓ All 256 bytes match!\n");
        return 0;
    } else {
        printf("\n✗ Outputs differ\n");
        return 1;
    }
}
