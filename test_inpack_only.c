/*
 * Test inpack module independently
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

extern void test_inpack_asm(uint64_t prewhite_key, uint8_t *output_ab,
                             uint8_t *output_cd, const uint8_t *input);

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input_16blks[256] = {0};
    uint8_t ab_asm[128], cd_asm[128];

    printf("========================================\n");
    printf("Testing INPACK Module Only\n");
    printf("========================================\n\n");

    /* Setup key to get pre-whitening key */
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    uint64_t prewhite_key = ctx.key_table[0];

    printf("Pre-whitening key: 0x%016lx\n\n", prewhite_key);

    /* Put test plaintext in first block, distinctive pattern in others */
    memcpy(input_16blks, test_plaintext, 16);
    for (int i = 1; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            input_16blks[i * 16 + j] = (i << 4) | j;
        }
    }

    printf("Input (first 4 blocks):\n");
    for (int i = 0; i < 4; i++) {
        printf("Block %d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", input_16blks[i * 16 + j]);
        }
        printf("\n");
    }

    /* Test ASM implementation */
    test_inpack_asm(prewhite_key, ab_asm, cd_asm, input_16blks);

    printf("\nASM inpack results:\n");
    printf("AB state (8 vectors, each 16 bytes):\n");
    for (int i = 0; i < 8; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", ab_asm[i * 16 + j]);
        }
        printf("\n");
    }

    printf("\nCD state (8 vectors, each 16 bytes):\n");
    for (int i = 0; i < 8; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", cd_asm[i * 16 + j]);
        }
        printf("\n");
    }

    printf("\n✓ inpack module executed\n");
    printf("Now we can compare this with C implementation manually\n");

    return 0;
}
