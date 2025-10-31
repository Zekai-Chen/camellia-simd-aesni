/*
 * Test the state after first roundsm16
 * Compare C vs Assembly implementations
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// External assembly function - returns state after byteslice + prewhiten + 1 round
extern void test_one_round_asm(uint8_t *ab_out, uint8_t *cd_out,
                                const uint8_t *input, const uint64_t *key_table);

// Test plaintext
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
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t ab_c[128], cd_c[128];
    uint8_t ab_asm[128], cd_asm[128];

    printf("========================================\n");
    printf("First Round Test (Byteslice + Prewhiten + Round 1)\n");
    printf("========================================\n\n");

    // Setup key
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }

    printf("Key schedule:\n");
    printf("  key_table[0]: 0x%016lx\n", ctx.key_table[0]);
    printf("  key_table[1]: 0x%016lx\n", ctx.key_table[2]);
    printf("  key_table[2]: 0x%016lx\n\n", ctx.key_table[2]);

    // Prepare input - 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    printf("Input (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Test C implementation
    printf("Running C implementation...\n");
    camellia_encrypt_16blks_simd128(&ctx, ab_c, cd_c, input);

    printf("C implementation - First 2 AB vectors:\n");
    print_vector("  ", 0, ab_c);
    print_vector("  ", 1, ab_c);
    printf("\n");

    // Test assembly implementation
    printf("Running assembly implementation...\n");
    test_one_round_asm(ab_asm, cd_asm, input, ctx.key_table);

    printf("Assembly - First 2 AB vectors:\n");
    print_vector("  ", 0, ab_asm);
    print_vector("  ", 1, ab_asm);
    printf("\n");

    // Compare
    printf("Comparison:\n");
    printf("-----------\n");
    int errors = 0;
    for (int vec = 0; vec < 8; vec++) {
        int vec_errors = 0;
        for (int byte = 0; byte < 16; byte++) {
            int idx = vec * 16 + byte;
            if (ab_c[idx] != ab_asm[idx]) {
                vec_errors++;
                errors++;
            }
        }
        if (vec_errors > 0) {
            printf("AB[%d]: %d bytes differ (first: C=0x%02x ASM=0x%02x)\n",
                   vec, vec_errors, ab_c[vec * 16], ab_asm[vec * 16]);
        } else {
            printf("AB[%d]: OK\n", vec);
        }
    }

    for (int vec = 0; vec < 8; vec++) {
        int vec_errors = 0;
        for (int byte = 0; byte < 16; byte++) {
            int idx = vec * 16 + byte;
            if (cd_c[idx] != cd_asm[idx]) {
                vec_errors++;
                errors++;
            }
        }
        if (vec_errors > 0) {
            printf("CD[%d]: %d bytes differ (first: C=0x%02x ASM=0x%02x)\n",
                   vec, vec_errors, cd_c[vec * 16], cd_asm[vec * 16]);
        } else {
            printf("CD[%d]: OK\n", vec);
        }
    }

    printf("\n");
    if (errors == 0) {
        printf("✓ SUCCESS: First round matches!\n");
        return 0;
    } else {
        printf("✗ FAILED: %d bytes differ after first round\n", errors);
        return 1;
    }
}
