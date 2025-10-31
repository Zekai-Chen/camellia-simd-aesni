/*
 * Component-by-component validation
 * Test each stage separately to identify where the divergence occurs
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

// External test functions
extern void test_prewhiten_asm(struct camellia_simd_ctx *ctx,
                                const void *input, void *output);
extern void test_first_roundsm16_asm(struct camellia_simd_ctx *ctx,
                                      const void *input, void *output_ab, void *output_cd);

void hex_dump(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int j = 0; j < len; j++) {
        if (j > 0 && j % 4 == 0) printf(" ");
        printf("%02x", data[j]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[128];
    uint8_t ab_out[128], cd_out[128];

    printf("========================================\n");
    printf("Component-by-Component Validation\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Prepare input (all blocks identical)
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    printf("Key table values:\n");
    printf("  key_table[0] (pre-whiten):  %016lx\n", ctx.key_table[0]);
    printf("  key_table[2] (round k=0):   %016lx\n", ctx.key_table[2]);
    printf("  key_table[3] (round k=1):   %016lx\n", ctx.key_table[3]);
    printf("  key_table[26] (post-whiten): %016lx\n\n", ctx.key_table[26]);

    // Test 1: Pre-whitening + Byteslice
    printf("TEST 1: Pre-whitening + Byteslice\n");
    test_prewhiten_asm(&ctx, input, output);
    hex_dump("  First block after prewhiten+byteslice", output, 16);
    printf("\n");

    // Test 2: First roundsm16 (AB->CD)
    printf("TEST 2: First roundsm16 (AB->CD, k=2)\n");
    test_first_roundsm16_asm(&ctx, input, ab_out, cd_out);
    hex_dump("  AB[0] output", ab_out, 16);
    hex_dump("  CD[0] output", cd_out, 16);
    printf("\n");

    return 0;
}
