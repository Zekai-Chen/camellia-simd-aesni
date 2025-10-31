#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "camellia_simd.h"

static const uint8_t test_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern uint8_t debug_cd_before_byteslice[128];
extern uint8_t debug_cd_after_byteslice[128];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];

    // All 16 blocks identical
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("Input (block 0): ");
    for (int i = 0; i < 16; i++) printf("%02x", input[i]);
    printf("\n");

    printf("After pre-whitening, blocks should XOR with key_table[0]\n");

    camellia_keysetup_simd128(&ctx, test_key, 16);
    printf("key_table[0] = 0x%016lx\n", ctx.key_table[0]);

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);

    printf("\nBlocks 8-15 BEFORE byteslice (v8-v15 raw input):\n");
    for (int i = 0; i < 8; i++) {
        printf("  v%-2d: ", i + 8);
        for (int j = 0; j < 16; j++) {
            printf("%02x", debug_cd_before_byteslice[i * 16 + j]);
        }
        printf("\n");
    }

    printf("\nCD AFTER byteslice (byte-sliced output):\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", debug_cd_after_byteslice[i * 16 + j]);
        }
        printf("\n");
    }

    // Check if any zeros in BEFORE data
    int before_has_zeros = 0;
    for (int i = 0; i < 128; i++) {
        if (debug_cd_before_byteslice[i] == 0) {
            before_has_zeros = 1;
            printf("\nZERO FOUND in BEFORE data at byte %d (vector v%d, offset %d)\n",
                   i, 8 + i / 16, i % 16);
            break;
        }
    }

    // Check if any zeros in AFTER data
    int after_has_zeros = 0;
    for (int i = 0; i < 128; i++) {
        if (debug_cd_after_byteslice[i] == 0) {
            after_has_zeros = 1;
            printf("\nZERO FOUND in AFTER data at CD byte %d (vector CD[%d], offset %d)\n",
                   i, i / 16, i % 16);
            break;
        }
    }

    if (!before_has_zeros && !after_has_zeros) {
        printf("\nNo zeros found - all data is OK\n");
    } else if (before_has_zeros) {
        printf("\n*** BUG: Input blocks have zeros BEFORE byteslice! ***\n");
    } else {
        printf("\n*** BUG: Zeros introduced BY byteslice transformation! ***\n");
    }

    return 0;
}
