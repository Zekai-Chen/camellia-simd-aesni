/*
 * Test to compare state after byteslice + pre-whitening
 * This helps identify if the divergence is in the initial setup
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

// External assembly functions
extern void test_byteslice_asm(uint8_t *output, const uint8_t *input);
extern void test_initial_state_asm(uint8_t *ab_out, uint8_t *cd_out,
                                   const uint8_t *input, const uint64_t *key_table);

// Test key and plaintext
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Helper functions
static void print_vector(const char *label, const uint8_t *data, int index) {
    printf("%s[%d]: ", label, index);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[index * 16 + i]);
    }
    printf("\n");
}

static void byteslice_c(uint8_t *ab, uint8_t *cd, const uint8_t *input) {
    // Use NEON intrinsics to replicate C implementation
    uint8x16_t v0, v1, v2, v3, v4, v5, v6, v7;
    uint8x16_t v8, v9, v10, v11, v12, v13, v14, v15;

    // Load 16 blocks
    v15 = vld1q_u8(input + 0 * 16);
    v14 = vld1q_u8(input + 1 * 16);
    v13 = vld1q_u8(input + 2 * 16);
    v12 = vld1q_u8(input + 3 * 16);
    v11 = vld1q_u8(input + 4 * 16);
    v10 = vld1q_u8(input + 5 * 16);
    v9 = vld1q_u8(input + 6 * 16);
    v8 = vld1q_u8(input + 7 * 16);
    v7 = vld1q_u8(input + 8 * 16);
    v6 = vld1q_u8(input + 9 * 16);
    v5 = vld1q_u8(input + 10 * 16);
    v4 = vld1q_u8(input + 11 * 16);
    v3 = vld1q_u8(input + 12 * 16);
    v2 = vld1q_u8(input + 13 * 16);
    v1 = vld1q_u8(input + 14 * 16);
    v0 = vld1q_u8(input + 15 * 16);

    // Simple transpose for testing
    // For now, just store them as-is (we'll use proper byteslice if needed)
    vst1q_u8(ab + 0 * 16, v0);
    vst1q_u8(ab + 1 * 16, v1);
    vst1q_u8(ab + 2 * 16, v2);
    vst1q_u8(ab + 3 * 16, v3);
    vst1q_u8(ab + 4 * 16, v4);
    vst1q_u8(ab + 5 * 16, v5);
    vst1q_u8(ab + 6 * 16, v6);
    vst1q_u8(ab + 7 * 16, v7);

    vst1q_u8(cd + 0 * 16, v8);
    vst1q_u8(cd + 1 * 16, v9);
    vst1q_u8(cd + 2 * 16, v10);
    vst1q_u8(cd + 3 * 16, v11);
    vst1q_u8(cd + 4 * 16, v12);
    vst1q_u8(cd + 5 * 16, v13);
    vst1q_u8(cd + 6 * 16, v14);
    vst1q_u8(cd + 7 * 16, v15);
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t plaintext[256];
    uint8_t ab_c[128], cd_c[128];
    uint8_t ab_asm[128], cd_asm[128];

    printf("========================================\n");
    printf("Pre-Whitening State Comparison\n");
    printf("========================================\n\n");

    // Setup key
    printf("Step 1: Key Setup\n");
    printf("-----------------\n");
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    printf("Key table[0] (kw1): 0x%016lx\n", ctx.key_table[0]);
    printf("Key table[1] (kw2): 0x%016lx\n", ctx.key_table[1]);
    printf("Key table[2] (k1):  0x%016lx\n\n", ctx.key_table[2]);

    // Prepare plaintext
    printf("Step 2: Prepare Plaintext\n");
    printf("-------------------------\n");
    for (int i = 0; i < 16; i++) {
        memcpy(plaintext + i * 16, test_plaintext, 16);
    }
    printf("Plaintext (all 16 blocks identical):\n");
    printf("  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Test byteslice
    printf("Step 3: Byteslice\n");
    printf("-----------------\n");
    test_byteslice_asm(ab_asm, plaintext);
    printf("Bytesliced structure (first 2 vectors):\n");
    print_vector("  ", ab_asm, 0);
    print_vector("  ", ab_asm, 1);
    printf("\n");

    // Test assembly: byteslice + pre-whitening
    printf("Step 4: Assembly - Byteslice + Pre-Whitening\n");
    printf("---------------------------------------------\n");
    test_initial_state_asm(ab_asm, cd_asm, plaintext, ctx.key_table);
    printf("Assembly AB (first 2 vectors):\n");
    print_vector("  ", ab_asm, 0);
    print_vector("  ", ab_asm, 1);
    printf("Assembly CD (first 2 vectors):\n");
    print_vector("  ", cd_asm, 0);
    print_vector("  ", cd_asm, 1);
    printf("\n");

    // Test C implementation for comparison
    printf("Step 5: C Implementation - Running Full Encryption\n");
    printf("---------------------------------------------------\n");
    uint8_t plaintext_copy[256];
    uint8_t ciphertext[256];
    memcpy(plaintext_copy, plaintext, 256);
    camellia_encrypt_16blks_simd128(&ctx, ciphertext, plaintext_copy);
    printf("C ciphertext (first block): ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", ciphertext[i]);
    }
    printf("\n\n");

    printf("========================================\n");
    printf("Analysis\n");
    printf("========================================\n");
    printf("This shows the AB/CD state after:\n");
    printf("1. Byteslicing 16 blocks\n");
    printf("2. Pre-whitening (AB ^= kw1, CD ^= kw2)\n");
    printf("\n");
    printf("The assembly AB/CD should match what C code has\n");
    printf("at the start of the first round.\n");
    printf("\n");
    printf("To verify correctness, we need to extract the\n");
    printf("corresponding state from C implementation.\n");

    return 0;
}
