/*
 * Direct roundsm16 test - bypassing byteslice
 * Create known byte-sliced AB state, run one roundsm16, compare outputs
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

// External assembly test function
extern void test_single_roundsm16_asm(struct camellia_simd_ctx *ctx,
                                       const void *ab_in, const void *cd_in,
                                       void *ab_out, void *cd_out, int k);

void print_vector(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", data[i]);
    }
    printf("\n");
}

void print_state(const char *label, const uint8_t *state) {
    printf("%s:\n", label);
    for (int i = 0; i < 8; i++) {
        printf("  [%d]: ", i);
        for (int j = 0; j < 16; j++) {
            if (j > 0 && j % 4 == 0) printf(" ");
            printf("%02x", state[i * 16 + j]);
        }
        printf("\n");
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    
    // Create simple byte-sliced AB state (each byte position has same value)
    uint8_t ab_in[128], cd_in[128];
    uint8_t ab_out_asm[128], cd_out_asm[128];
    
    printf("========================================\n");
    printf("Direct Roundsm16 Test (No Byteslice)\n");
    printf("========================================\n\n");
    
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    
    printf("Testing with k=2 (key_table[2] = %016lx)\n\n", ctx.key_table[2]);
    
    // Create test pattern matching C code's AB after inpack (zero input + pre-whitening)
    // These values are from C debug output
    const uint8_t ab_values[8] = {0xaf, 0x52, 0x86, 0xb2, 0xd2, 0x0d, 0x72, 0xf2};
    for (int i = 0; i < 8; i++) {
        memset(&ab_in[i * 16], ab_values[i], 16);
    }
    
    // CD = all zeros
    memset(cd_in, 0, 128);
    
    printf("Input AB state (byte-sliced):\n");
    print_state("  AB", ab_in);
    printf("\nInput CD state (all zeros):\n");
    printf("  CD[0]: ");
    print_vector("", cd_in);
    printf("\n");
    
    // Test assembly version
    test_single_roundsm16_asm(&ctx, ab_in, cd_in, ab_out_asm, cd_out_asm, 2);
    
    printf("Assembly Output CD state:\n");
    print_state("  CD", cd_out_asm);
    
    printf("\n========================================\n");
    printf("Now testing C reference implementation\n");
    printf("========================================\n\n");
    
    // For C reference, use ZERO input to match assembly test
    uint8_t input[256], output[256];
    memset(input, 0, 256);
    
    // Run full C encryption to get comparison point
    camellia_encrypt_16blks_simd128(&ctx, output, input);
    
    printf("C full encryption output (first block):\n");
    printf("  Block 0: ");
    print_vector("", output);
    
    return 0;
}
