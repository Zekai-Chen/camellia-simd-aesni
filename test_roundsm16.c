/*
 * Unit test for roundsm16 macro
 * Tests a single round invocation with known inputs
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

// External assembly function wrapper
extern void test_roundsm16_asm(uint8_t *ab_out, uint8_t *cd_in,
                               const uint64_t *key_table, uint32_t round_idx);

// Helper to print 16 bytes
static void print_vector(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

// Test key and data
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Create test AB input (after byteslice)
static void create_test_ab(uint8_t ab[128]) {
    // Initialize with a simple pattern for each of 8 vectors
    for (int vec = 0; vec < 8; vec++) {
        for (int byte = 0; byte < 16; byte++) {
            ab[vec * 16 + byte] = (uint8_t)((vec * 16 + byte) & 0xff);
        }
    }
}

// Create test CD input (after byteslice)
static void create_test_cd(uint8_t cd[128]) {
    // Initialize with a different pattern
    for (int vec = 0; vec < 8; vec++) {
        for (int byte = 0; byte < 16; byte++) {
            cd[vec * 16 + byte] = (uint8_t)((128 + vec * 16 + byte) & 0xff);
        }
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t ab_in[128];
    uint8_t cd_in[128];
    uint8_t ab_out_asm[128];

    printf("========================================\n");
    printf("roundsm16 Macro Unit Test\n");
    printf("========================================\n\n");

    // Step 1: Setup key
    printf("Step 1: Key Setup\n");
    printf("-----------------\n");
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    printf("Key setup successful\n");
    printf("key_table[0]: 0x%016lx\n", ctx.key_table[0]);
    printf("key_table[1]: 0x%016lx\n", ctx.key_table[1]);
    printf("key_table[2]: 0x%016lx\n\n", ctx.key_table[2]);

    // Step 2: Create test inputs
    printf("Step 2: Create Test Inputs\n");
    printf("--------------------------\n");
    create_test_ab(ab_in);
    create_test_cd(cd_in);

    printf("AB input (first 2 vectors):\n");
    print_vector("  v0", ab_in + 0 * 16);
    print_vector("  v1", ab_in + 1 * 16);
    printf("\n");

    printf("CD input (first 2 vectors):\n");
    print_vector("  v0", cd_in + 0 * 16);
    print_vector("  v1", cd_in + 1 * 16);
    printf("\n");

    // Step 3: Test assembly roundsm16
    printf("Step 3: Test Assembly roundsm16\n");
    printf("--------------------------------\n");

    // Test with round 0 (uses key_table[0])
    test_roundsm16_asm(ab_out_asm, cd_in, ctx.key_table, 0);

    printf("AB output from assembly (first 2 vectors):\n");
    print_vector("  v0", ab_out_asm + 0 * 16);
    print_vector("  v1", ab_out_asm + 1 * 16);
    printf("\n");

    // For now, just verify it runs without crashing
    printf("Test completed successfully - assembly executed without errors\n");
    printf("\nNext step: Extract C implementation intermediate values to compare\n");

    return 0;
}
