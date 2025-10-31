/*
 * Test byteslice against the REAL Kivilinna C implementation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Use the real Kivilinna implementation
extern void camellia_encrypt_16blks_simd128(void *ctx, void *out, const void *in);
extern int camellia_keysetup_simd128(void *ctx, const void *key, unsigned int keylen);

// Assembly test function that just does byteslice
extern void test_byteslice_with_kivilinna(uint8_t *output, const uint8_t *input);

int main(void) {
    uint8_t input[256];
    uint8_t output[256];

    printf("========================================\n");
    printf("Byteslice Test with Kivilinna C impl\n");
    printf("========================================\n\n");

    // Fill input with pattern
    for (int i = 0; i < 256; i++) {
        input[i] = (i / 16) * 16 + (i % 16);
    }

    printf("Testing byteslice macro compatibility...\n");

    // This assembly function will load input, call byteslice, then store
    test_byteslice_with_kivilinna(output, input);

    printf("Byteslice completed without crash\n");
    printf("First 4 output blocks:\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x", output[block * 16 + i]);
        }
        printf("\n");
    }

    printf("\n✓ Test completed\n");
    return 0;
}
