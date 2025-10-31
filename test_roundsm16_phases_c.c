/*
 * Phase-by-phase C reference for roundsm16
 * This manually implements each phase to get reference values
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

void print_vector(const char *label, uint8x16_t v) {
    uint8_t data[16];
    vst1q_u8(data, v);
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", data[i]);
    }
    printf("\n");
}

// Simple test: all-zero AB state, see what first roundsm16 produces
int main(void) {
    struct camellia_simd_ctx ctx;
    
    printf("========================================\n");
    printf("C Reference: Roundsm16 Phase-by-Phase\n");
    printf("========================================\n\n");
    
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    
    printf("Test: Zero AB input, first round (k=2)\n");
    printf("Key table[2]: %016lx\n\n", ctx.key_table[2]);
    
    // Create zero AB state (8 vectors)
    uint8x16_t ab[8];
    for (int i = 0; i < 8; i++) {
        ab[i] = vdupq_n_u8(0);
    }
    
    printf("Input AB[0] (all should be zero):\n");
    print_vector("  AB[0]", ab[0]);
    print_vector("  AB[7]", ab[7]);
    
    printf("\nNow running C implementation...\n");
    
    // Load 16 blocks of zeros
    uint8_t input[256];
    memset(input, 0, 256);
    
    uint8_t output[256];
    camellia_encrypt_16blks_simd128(&ctx, output, input);
    
    printf("\nC full encryption output (first block):\n");
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", output[i]);
    }
    printf("\n");
    
    return 0;
}
