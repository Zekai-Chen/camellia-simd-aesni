/*
 * Test assembly roundsm16 with zero input
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                          void *out, const void *in);

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];
    
    printf("========================================\n");
    printf("Assembly: Zero Input Test\n");
    printf("========================================\n\n");
    
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    
    printf("Key table[2]: %016lx\n\n", ctx.key_table[2]);
    
    // Zero input
    memset(input, 0, 256);
    
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);
    
    printf("Assembly output (first block):\n");
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", output[i]);
    }
    printf("\n");
    
    return 0;
}
