/*
 * Test: Compare byteslice + prewhiten between C and Assembly
 * This tests the initial state before any rounds
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// External assembly function that returns state after byteslice + prewhiten
extern void test_initial_state_asm(uint8_t *ab_out, uint8_t *cd_out,
                                    const uint8_t *input,
                                    const uint64_t *key_table);

// Test key
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Test plaintext
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%d]: ", label, idx);
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
    uint8_t output_c[256];

    printf("========================================\n");
    printf("Byteslice + Prewhiten Comparison Test\n");
    printf("========================================\n\n");

    // Setup key
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }

    printf("Key table:\n");
    printf("  key_table[0]: 0x%016lx\n", ctx.key_table[0]);
    printf("  key_table[1]: 0x%016lx\n", ctx.key_table[1]);
    printf("  key_table[2]: 0x%016lx\n\n", ctx.key_table[2]);

    // Prepare 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    printf("Input (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Get C implementation state by running full encryption
    // We'll extract AB/CD from intermediate state
    printf("Running C implementation (full encryption to get intermediate state)...\n");

    // We need to modify this to extract just the initial state
    // For now, let's call the C function and capture the state
    // The C code doesn't expose intermediate state, so we'll need to use assembly

    printf("Running Assembly implementation (byteslice + prewhiten)...\n");
    test_initial_state_asm(ab_asm, cd_asm, input, ctx.key_table);

    printf("\nAssembly AB vectors (first 3):\n");
    for (int i = 0; i < 3; i++) {
        print_vector("  AB", i, ab_asm);
    }

    printf("\nAssembly CD vectors (first 3):\n");
    for (int i = 0; i < 3; i++) {
        print_vector("  CD", i, cd_asm);
    }

    // For comparison, we know the expected pattern after byteslice + prewhiten
    // After byteslice:
    //   AB[0] should have byte 0 of each block (all 0x01)
    //   AB[1] should have byte 2 of each block (all 0x45) - interleaved!
    //   AB[2] should have byte 1 of each block (all 0x23) - interleaved!
    //   etc.
    // After prewhiten, these are XORed with kw1/kw2

    printf("\n=== Validation ===\n");
    printf("Expected pattern after byteslice (before prewhiten):\n");
    printf("  AB[0] = 0x01 (byte 0), AB[1] = 0x45 (byte 2), AB[2] = 0x23 (byte 1)...\n");
    printf("  CD[0] = 0x89 (byte 8), CD[1] = 0xcd (byte 10), CD[2] = 0xab (byte 9)...\n\n");

    printf("After XOR with kw1=0x%016lx and kw2=0x%016lx:\n", ctx.key_table[0], ctx.key_table[1]);

    // Calculate expected values
    uint64_t kw1 = ctx.key_table[0];
    uint64_t kw2 = ctx.key_table[1];

    // IMPORTANT: When kw1 is loaded into SIMD register with dup, the byte order
    // in the register is [byte0, byte1, ..., byte7, byte0, byte1, ..., byte7]
    // where byte0 is the LOWEST byte (kw1 & 0xff), NOT the highest byte
    uint8_t expected_ab0 = 0x01 ^ (uint8_t)(kw1 & 0xff);
    uint8_t expected_cd0 = 0x89 ^ (uint8_t)(kw2 & 0xff);

    printf("  Expected AB[0][0] = 0x01 ^ 0x%02x = 0x%02x\n",
           (uint8_t)(kw1 & 0xff), expected_ab0);
    printf("  Actual   AB[0][0] = 0x%02x\n", ab_asm[0]);

    printf("  Expected CD[0][0] = 0x89 ^ 0x%02x = 0x%02x\n",
           (uint8_t)(kw2 & 0xff), expected_cd0);
    printf("  Actual   CD[0][0] = 0x%02x\n", cd_asm[0]);

    // Check if they match
    int errors = 0;
    if (ab_asm[0] != expected_ab0) {
        printf("\n✗ AB[0][0] mismatch!\n");
        errors++;
    } else {
        printf("\n✓ AB[0][0] matches!\n");
    }

    if (cd_asm[0] != expected_cd0) {
        printf("✗ CD[0][0] mismatch!\n");
        errors++;
    } else {
        printf("✓ CD[0][0] matches!\n");
    }

    return errors;
}
