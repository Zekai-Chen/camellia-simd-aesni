/*
 * Test the state after byteslice + pre-whitening
 * This isolates whether the issue is in byteslice/prewhiten or in the rounds
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// External assembly function
extern void test_initial_state_asm(uint8_t *ab_out, uint8_t *cd_out,
                                   const uint8_t *input, const uint64_t *key_table);

// Test plaintext - 16 identical blocks
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Test key
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%2d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[idx * 16 + i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx_c, ctx_asm;
    uint8_t input[256];
    uint8_t ab_c[128], cd_c[128];
    uint8_t ab_asm[128], cd_asm[128];

    printf("========================================\n");
    printf("Initial State Test (Byteslice + Prewhiten)\n");
    printf("========================================\n\n");

    // Setup key
    if (camellia_keysetup_simd128(&ctx_c, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    ctx_asm = ctx_c;  // Copy key schedule

    // Prepare input - 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    printf("Input (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    printf("Key schedule:\n");
    printf("  key_table[0]: 0x%016lx\n", ctx_c.key_table[0]);
    printf("  key_table[1]: 0x%016lx\n\n", ctx_c.key_table[1]);

    // Test assembly implementation
    test_initial_state_asm(ab_asm, cd_asm, input, ctx_asm.key_table);

    printf("Assembly output (after byteslice + prewhiten):\n");
    printf("AB vectors:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, ab_asm);
    }
    printf("\nCD vectors:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, cd_asm);
    }
    printf("\n");

    // TODO: Add C reference implementation to compare
    printf("Next step: Implement C reference for initial state\n");

    return 0;
}
