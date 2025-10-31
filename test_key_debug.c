#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern uint8_t debug_v0[16];
extern uint8_t debug_v1[16];
extern uint8_t debug_v2[16];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    printf("key_table[0] = 0x%016lx\n", ctx.key_table[0]);

    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);

    printf("\nDEBUG: v31 after pack_bswap: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", debug_v0[i]);
    }
    printf("\n");

    printf("Expected pack_bswap(key_table[0]):\n");
    printf("  Original: d20d72f2af5286b2\n");
    printf("  After pack_bswap: b2 86 52 af f2 72 0d d2  0f 0f 0f 0f 0f 0f 0f 0f\n");

    printf("\nDEBUG: v0 before byteslice (block 0 after pre-whitening): ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", debug_v1[i]);
    }
    printf("\n");

    printf("DEBUG: v1 before byteslice (block 1 after pre-whitening): ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", debug_v2[i]);
    }
    printf("\n");

    printf("\nExpected block 0 XOR key: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", test_input[i] ^ ((uint8_t*)&ctx.key_table[0])[i % 8]);
    }
    printf("\n");

    return 0;
}
