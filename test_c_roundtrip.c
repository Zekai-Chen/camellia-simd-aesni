/*
 * Test C implementation roundtrip
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t intermediate[256];
    uint8_t output[256];

    // Test key (not used, just for initialization)
    static const uint8_t test_key[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
    };

    printf("Testing C implementation roundtrip\n");
    printf("===================================\n\n");

    // Fill input with distinct pattern
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }

    printf("Input block 0: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", input[i]);
    }
    printf("\n\n");

    // Initialize context (needed for structure)
    camellia_keysetup_simd128(&ctx, test_key, 16);

    // Encrypt then decrypt (should be roundtrip)
    camellia_encrypt_16blks_simd128(&ctx, intermediate, input);
    camellia_decrypt_16blks_simd128(&ctx, output, intermediate);

    printf("After encrypt+decrypt block 0: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", output[i]);
    }
    printf("\n\n");

    // Compare
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (input[i] != output[i]) {
            errors++;
        }
    }

    printf("Result: %d/256 bytes match\n", 256 - errors);
    printf("%s\n", errors == 0 ? "✓ C ROUNDTRIP OK" : "✗ C ROUNDTRIP FAILED");

    return errors == 0 ? 0 : 1;
}
