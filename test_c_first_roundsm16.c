/*
 * C test with instrumentation to capture first roundsm16 output
 * We'll temporarily modify the C implementation to add debug output
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

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

    printf("========================================\n");
    printf("C Test: Capture First Roundsm16 Output\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    printf("Key table[2] = %016lx\n", ctx.key_table[2]);
    printf("Key table[3] = %016lx\n\n", ctx.key_table[3]);

    // Create input that will produce uniform byte-sliced AB after inpack
    // Each block has the same pattern: byte i = i*0x11
    uint8_t input[256];
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = byte * 0x11;
        }
    }

    printf("Input pattern (each block identical):\n");
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", input[i]);
    }
    printf("\n\n");

    // This will produce byte-sliced AB where AB[i] represents
    // byte position i from all 16 blocks
    // Since all blocks are identical, after byteslicing:
    // AB[0] = byte 0 from all blocks = all same value
    // But this is different from our assembly test pattern!

    printf("Note: This input produces different byte-sliced pattern than assembly test\n");
    printf("      Assembly test: AB[i] = all (i*0x11)\n");
    printf("      This input: AB contains byte positions, not bit-sliced\n\n");

    uint8_t output[256];
    camellia_encrypt_16blks_simd128(&ctx, output, input);

    printf("Full encryption output (first block):\n");
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", output[i]);
    }
    printf("\n");

    return 0;
}
