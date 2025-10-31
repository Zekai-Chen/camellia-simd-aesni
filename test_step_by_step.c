/*
 * Step-by-step comparison of C vs Assembly implementation
 * This will help us identify exactly where they diverge
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// External test functions
extern void test_byteslice_asm(uint8_t *output, const uint8_t *input);

// Helper to print vectors
static void print_state(const char *label, const uint8_t *data, int count) {
    printf("%s:\n", label);
    for (int i = 0; i < count; i++) {
        printf("  [%2d]: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x ", data[i * 16 + j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Test key and data (same as test_basic.c)
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t plaintext[256];  // 16 blocks
    uint8_t ciphertext_c[256];
    uint8_t ciphertext_asm[256];
    uint8_t bytesliced[256];

    printf("========================================\n");
    printf("Step-by-Step Camellia Debugging\n");
    printf("========================================\n\n");

    // Step 1: Key setup
    printf("Step 1: Key Setup\n");
    printf("-----------------\n");
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    printf("Key setup successful\n");
    printf("Key length: %d bytes\n", ctx.key_length);
    printf("First round key (key_table[0]): %016lx\n", ctx.key_table[0]);
    printf("Second round key (key_table[1]): %016lx\n", ctx.key_table[1]);
    printf("\n");

    // Step 2: Prepare plaintext
    printf("Step 2: Prepare Plaintext\n");
    printf("-------------------------\n");
    for (int i = 0; i < 16; i++) {
        memcpy(plaintext + i * 16, test_plaintext, 16);
    }
    print_state("Input (first 4 blocks)", plaintext, 4);

    // Step 3: Test C implementation
    printf("Step 3: C Implementation\n");
    printf("------------------------\n");
    camellia_encrypt_16blks_simd128(&ctx, ciphertext_c, plaintext);
    print_state("C Output (first 4 blocks)", ciphertext_c, 4);

    // Step 4: Test byteslicing
    printf("Step 4: Test Byteslice\n");
    printf("----------------------\n");
    test_byteslice_asm(bytesliced, plaintext);
    printf("Bytesliced data structure created\n");
    print_state("Bytesliced (first 4 vectors)", bytesliced, 4);

    // Step 5: Test assembly implementation
    printf("Step 5: Assembly Implementation\n");
    printf("-------------------------------\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, ciphertext_asm, plaintext);
    print_state("Assembly Output (first 4 blocks)", ciphertext_asm, 4);

    // Step 6: Compare
    printf("Step 6: Comparison\n");
    printf("------------------\n");
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (ciphertext_c[i] != ciphertext_asm[i]) {
            if (errors < 10) {  // Only print first 10 errors
                printf("Mismatch at byte %d: C=0x%02x ASM=0x%02x\n",
                       i, ciphertext_c[i], ciphertext_asm[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("SUCCESS: All bytes match!\n");
        return 0;
    } else {
        printf("FAILED: %d bytes differ\n", errors);
        return 1;
    }
}
