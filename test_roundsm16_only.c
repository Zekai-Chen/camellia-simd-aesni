/*
 * Test ONLY the roundsm16 macro with known inputs
 * Compare C vs Assembly implementation
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

void print_vector(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int j = 0; j < 16; j++) {
        printf("%02x", data[j]);
    }
    printf("\n");
}

// C implementation of roundsm16 using the same logic as the macro
// This is extracted from the C intrinsics implementation
extern void roundsm16_c_wrapper(uint8_t *ab_out, uint8_t *cd_out,
                                 const uint8_t *ab_in, const uint8_t *cd_in,
                                 uint64_t round_key);

int main(void) {
    struct camellia_simd_ctx ctx;

    printf("========================================\n");
    printf("Roundsm16 Isolated Test\n");
    printf("========================================\n\n");

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    uint64_t round_key = ctx.key_table[2];  // First round key in encryption

    printf("Round key (key_table[2]): %016lx\n\n", round_key);

    // Create simple test vectors for AB and CD
    uint8_t ab_in[128], cd_in[128];

    // Initialize with distinctive patterns
    for (int i = 0; i < 128; i++) {
        ab_in[i] = (uint8_t)(i & 0xFF);
        cd_in[i] = (uint8_t)((i ^ 0xFF) & 0xFF);
    }

    printf("Input AB (first vector):\n");
    print_vector("  AB[0]", &ab_in[0]);

    printf("Input CD (first vector):\n");
    print_vector("  CD[0]", &cd_in[0]);
    printf("\n");

    // For now, just show the setup
    // The actual C implementation would require extracting roundsm16 logic
    printf("Note: This test verifies the setup. Full C comparison requires\n");
    printf("extracting roundsm16 from the intrinsics implementation.\n");

    printf("\nKey bytes extracted from round_key:\n");
    for (int i = 0; i < 8; i++) {
        uint8_t byte = (round_key >> (i * 8)) & 0xFF;
        printf("  Byte[%d]: %02x\n", i, byte);
    }

    return 0;
}
