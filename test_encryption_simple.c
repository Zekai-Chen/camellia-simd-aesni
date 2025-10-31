/*
 * Simple end-to-end encryption test
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

void print_hex(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len) printf("\n%*s  ", (int)strlen(label), "");
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx_c, ctx_asm;
    uint8_t input[256], output_c[256], output_asm[256];

    printf("========================================\n");
    printf("End-to-End Encryption Test\n");
    printf("========================================\n\n");

    // Initialize: all 16 blocks with same plaintext
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    // Test C reference implementation
    printf("Testing C reference implementation...\n");
    camellia_keysetup_simd128(&ctx_c, test_key, 16);
    camellia_encrypt_16blks_simd128(&ctx_c, output_c, input);

    printf("C reference output (first block):\n");
    print_hex("  Block 0", output_c, 16);

    // Test Assembly implementation
    printf("\nTesting Assembly implementation...\n");
    camellia_keysetup_simd128(&ctx_asm, test_key, 16);
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx_asm, output_asm, input);

    printf("Assembly output (first block):\n");
    print_hex("  Block 0", output_asm, 16);

    // Compare
    printf("\nComparing outputs...\n");
    int match = 1;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            if (match) {
                printf("MISMATCH at byte %d (block %d, offset %d):\n",
                       i, i / 16, i % 16);
                printf("  C:   %02x\n", output_c[i]);
                printf("  ASM: %02x\n", output_asm[i]);
            }
            match = 0;
        }
    }

    if (match) {
        printf("SUCCESS: Assembly output matches C reference!\n");
        printf("\nAll 16 blocks encrypted correctly:\n");
        for (int i = 0; i < 16; i++) {
            printf("  Block %2d: ", i);
            for (int j = 0; j < 16; j++) {
                printf("%02x", output_asm[i * 16 + j]);
            }
            printf("\n");
        }
        return 0;
    } else {
        printf("FAILURE: Assembly output differs from C reference\n");
        return 1;
    }
}
