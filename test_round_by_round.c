/*
 * Round-by-round comparison test
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern uint8_t debug_ab_before_first_round[128];
extern uint8_t debug_cd_after_byteslice[128];
extern uint8_t debug_cd_after_first_round[128];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

void print_vec(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx_c, ctx_asm;
    uint8_t input[256], output_c[256], output_asm[256];

    printf("========================================\n");
    printf("Round-by-Round Comparison Test\n");
    printf("========================================\n\n");

    // Initialize: all 16 blocks with same plaintext
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    // Test C reference
    printf("=== C REFERENCE ===\n");
    camellia_keysetup_simd128(&ctx_c, test_key, 16);
    camellia_encrypt_16blks_simd128(&ctx_c, output_c, input);
    printf("C output block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_c[i]);
    printf("\n");

    // Test Assembly
    printf("\n=== ASSEMBLY ===\n");
    camellia_keysetup_simd128(&ctx_asm, test_key, 16);
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx_asm, output_asm, input);

    printf("Assembly output block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_asm[i]);
    printf("\n");

    // Compare
    int match = 1;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            match = 0;
            break;
        }
    }

    if (match) {
        printf("\nSUCCESS: All blocks match!\n");
        return 0;
    } else {
        printf("\nMismatch detected. Adding more debug...\n");
        return 1;
    }
}
