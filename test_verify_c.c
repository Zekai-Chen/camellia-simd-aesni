/*
 * Verify C implementation against RFC 3713 test vectors
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

/* RFC 3713 test vectors */
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_ciphertext_128[16] = {
    0x67, 0x67, 0x31, 0x38, 0x54, 0x96, 0x69, 0x73,
    0x08, 0x57, 0x06, 0x56, 0x48, 0xea, 0xbe, 0x43
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input_16blks[256] = {0};
    uint8_t output_16blks[256];
    uint8_t output[16];

    printf("========================================\n");
    printf("Verifying C Implementation\n");
    printf("RFC 3713 Test Vectors\n");
    printf("========================================\n\n");

    /* Setup key */
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    /* Put test plaintext in first block */
    memcpy(input_16blks, test_plaintext, 16);

    /* Encrypt 16 blocks */
    camellia_encrypt_16blks_simd128(&ctx, output_16blks, input_16blks);

    /* Extract first block */
    memcpy(output, output_16blks, 16);

    /* Display results */
    printf("Key:        ");
    for (int i = 0; i < 16; i++) printf("%02x", test_key_128[i]);
    printf("\n");

    printf("Plaintext:  ");
    for (int i = 0; i < 16; i++) printf("%02x", test_plaintext[i]);
    printf("\n");

    printf("C Output:   ");
    for (int i = 0; i < 16; i++) printf("%02x", output[i]);
    printf("\n");

    printf("Expected:   ");
    for (int i = 0; i < 16; i++) printf("%02x", test_ciphertext_128[i]);
    printf("\n\n");

    /* Verify */
    if (memcmp(output, test_ciphertext_128, 16) == 0) {
        printf("✓ C implementation CORRECT (matches RFC 3713)\n");
        return 0;
    } else {
        printf("✗ C implementation INCORRECT\n");

        /* Show differences */
        printf("\nDifferences:\n");
        for (int i = 0; i < 16; i++) {
            if (output[i] != test_ciphertext_128[i]) {
                printf("  Byte %2d: got %02x, expected %02x\n",
                       i, output[i], test_ciphertext_128[i]);
            }
        }
        return 1;
    }
}
