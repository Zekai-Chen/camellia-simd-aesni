/*
 * Test to capture state after byteslice (before any rounds)
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Already have this function from before
extern void test_capture_after_byteslice(uint8_t *ab_out, uint8_t *cd_out,
                                         const uint8_t *input, uint64_t prewhiten_key);

void print_state(const char *label, const uint8_t *data, int num_vectors) {
    printf("%s:\n", label);
    for (int i = 0; i < num_vectors && i < 8; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", data[i * 16 + j]);
        }
        printf("\n");
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t ab[128], cd[128];

    printf("========================================\n");
    printf("State After Byteslice\n");
    printf("========================================\n\n");

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    uint64_t key = ctx.key_table[0];

    printf("Pre-whiten key: %016lx\n\n", key);

    // Prepare input (all blocks identical)
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    // Capture state after byteslice
    test_capture_after_byteslice(ab, cd, input, key);

    print_state("AB state (after byteslice)", ab, 8);
    printf("\n");
    print_state("CD state (after byteslice)", cd, 8);

    printf("\n✓ Test completed\n");
    return 0;
}
