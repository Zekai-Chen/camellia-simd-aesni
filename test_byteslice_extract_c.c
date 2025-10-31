/*
 * Extract actual byteslice output from C implementation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

// Test plaintext
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// External assembly function
extern void test_initial_state_asm(uint8_t *ab_out, uint8_t *cd_out,
                                   const uint8_t *input, const uint64_t *key_table);

static void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%2d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[idx * 16 + i]);
    }
    printf("\n");
}

// Hook into C encryption to extract byteslice output
static uint8_t captured_ab[128];
static uint8_t captured_cd[128];
static int capture_done = 0;

// We'll modify C code temporarily to capture state
// For now, let's just look at what assembly produces vs what we expect

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t plaintext[256];
    uint8_t ab_asm[128], cd_asm[128];

    printf("========================================\n");
    printf("Extract Byteslice from C Implementation\n");
    printf("========================================\n\n");

    // Key setup
    if (camellia_keysetup_simd128(&ctx, test_plaintext, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }

    // Prepare plaintext - 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(plaintext + i * 16, test_plaintext, 16);
    }

    printf("Input plaintext (all 16 blocks identical):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Get assembly implementation output (after byteslice + prewhiten)
    printf("Assembly Implementation (byteslice + prewhiten):\n");
    printf("------------------------------------------------\n");
    test_initial_state_asm(ab_asm, cd_asm, plaintext, ctx.key_table);

    printf("AB (first 4 vectors):\n");
    for (int i = 0; i < 4; i++) {
        print_vector("  ", i, ab_asm);
    }
    printf("\n");

    printf("CD (first 4 vectors):\n");
    for (int i = 0; i < 4; i++) {
        print_vector("  ", i, cd_asm);
    }
    printf("\n");

    // Expected format analysis
    printf("Analysis:\n");
    printf("---------\n");
    printf("Based on C code structure (from inpack16_post):\n");
    printf("- AB should contain bytes 0-7 of all blocks (left 64 bits)\n");
    printf("- CD should contain bytes 8-15 of all blocks (right 64 bits)\n");
    printf("\n");
    printf("For 16 identical input blocks, after byteslice:\n");
    printf("- ab[0] should have all byte-0 values (0x01)\n");
    printf("- ab[1] should have all byte-1 values (0x23)\n");
    printf("- ...\n");
    printf("- cd[0] should have all byte-8 values (0xfe)\n");
    printf("- cd[1] should have all byte-9 values (0xdc)\n");
    printf("\n");
    printf("After XOR with pre-whitening key (kw1=0x%016lx, kw2=0x%016lx):\n",
           ctx.key_table[0], ctx.key_table[1]);
    printf("Values will change based on key XOR.\n");

    return 0;
}
