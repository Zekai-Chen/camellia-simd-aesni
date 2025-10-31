/*
 * Verify ASM implementation against RFC 3713 test vectors
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                        void *out, const void *in);

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
    uint8_t output_c[256];
    uint8_t output_asm[256];
    uint8_t result_c[16];
    uint8_t result_asm[16];

    printf("========================================\n");
    printf("Comparing C vs ASM with RFC 3713\n");
    printf("========================================\n\n");

    /* Setup key */
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    /* Put test plaintext in first block */
    memcpy(input_16blks, test_plaintext, 16);

    /* Encrypt with C */
    camellia_encrypt_16blks_simd128(&ctx, output_c, input_16blks);
    memcpy(result_c, output_c, 16);

    /* Encrypt with ASM */
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input_16blks);
    memcpy(result_asm, output_asm, 16);

    /* Display results */
    printf("Key:        ");
    for (int i = 0; i < 16; i++) printf("%02x", test_key_128[i]);
    printf("\n");

    printf("Plaintext:  ");
    for (int i = 0; i < 16; i++) printf("%02x", test_plaintext[i]);
    printf("\n");

    printf("Expected:   ");
    for (int i = 0; i < 16; i++) printf("%02x", test_ciphertext_128[i]);
    printf("\n");

    printf("C Output:   ");
    for (int i = 0; i < 16; i++) printf("%02x", result_c[i]);
    printf(" %s\n", memcmp(result_c, test_ciphertext_128, 16) == 0 ? "✓" : "✗");

    printf("ASM Output: ");
    for (int i = 0; i < 16; i++) printf("%02x", result_asm[i]);
    printf(" %s\n", memcmp(result_asm, test_ciphertext_128, 16) == 0 ? "✓" : "✗");

    printf("\n");

    /* Verify ASM */
    if (memcmp(result_asm, test_ciphertext_128, 16) == 0) {
        printf("✓✓✓ ASM implementation CORRECT! ✓✓✓\n");
        return 0;
    } else {
        printf("✗ ASM implementation incorrect\n\n");

        /* Show byte-by-byte comparison */
        printf("Byte-by-byte comparison:\n");
        printf("Pos  Expected  C-Out  ASM-Out  C-Match  ASM-Match\n");
        for (int i = 0; i < 16; i++) {
            printf("%2d:    %02x      %02x      %02x       %s        %s\n",
                   i,
                   test_ciphertext_128[i],
                   result_c[i],
                   result_asm[i],
                   result_c[i] == test_ciphertext_128[i] ? "✓" : "✗",
                   result_asm[i] == test_ciphertext_128[i] ? "✓" : "✗");
        }
        return 1;
    }
}
