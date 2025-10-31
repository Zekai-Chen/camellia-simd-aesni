#include <stdio.h>
#include <string.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Initialize input: all blocks identical
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    // Call C encryption (will print debug output)
    camellia_encrypt_16blks_simd128(&ctx, output, input);

    return 0;
}
