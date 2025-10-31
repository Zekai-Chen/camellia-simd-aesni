/*
 * Debug test: Check why CD has zeros in last 4 bytes
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t zero_input[256] = {0}; // All zeros

extern uint8_t debug_ab_before_first_round[128];
extern uint8_t debug_cd_after_byteslice[128];
extern uint8_t debug_byteslice_input_v8[16];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

void print_hex(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i+1) % 16 == 0 && i+1 < len) printf(" ");
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t dummy_key[16] = {0};  // Zero key
    uint8_t output[256];

    camellia_keysetup_simd128(&ctx, dummy_key, 16);

    printf("========================================\n");
    printf("CD Zeros Debug Test\n");
    printf("========================================\n\n");

    printf("Using zero input and zero key for simplicity\n");
    printf("key_table[0] = 0x%016lx\n\n", ctx.key_table[0]);

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, zero_input);

    printf("Block 12 BEFORE byteslice (should be all 6c after pre-whitening):\n  ");
    print_hex("", debug_byteslice_input_v8, 16);
    printf("\n");

    printf("CD after byteslice:\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        print_hex("", &debug_cd_after_byteslice[i * 16], 16);
    }

    printf("\nAnalysis:\n");
    printf("  If block 12 has all 6c before byteslice, input is OK\n");
    printf("  If CD last 4 bytes are 00, there's a bug in byteslice itself\n");

    return 0;
}
