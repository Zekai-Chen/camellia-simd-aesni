/*
 * C version of component tests for comparison
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

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

void hex_dump(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int j = 0; j < len; j++) {
        if (j > 0 && j % 4 == 0) printf(" ");
        printf("%02x", data[j]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8x16_t blocks[16];
    uint8x16_t key;

    printf("========================================\n");
    printf("C Component Test\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Prepare input
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    printf("Key table[0] (pre-whiten): %016lx\n", ctx.key_table[0]);
    printf("Input block 0: ");
    hex_dump("", test_plaintext, 16);
    printf("\n");

    // Load and pre-whiten
    key = vreinterpretq_u8_u64(vdupq_n_u64(ctx.key_table[0]));
    for (int i = 0; i < 16; i++) {
        blocks[i] = vld1q_u8(&input[i * 16]);
        blocks[i] = veorq_u8(blocks[i], key);
    }

    printf("After pre-whiten, block 0: ");
    uint8_t temp[16];
    vst1q_u8(temp, blocks[0]);
    hex_dump("", temp, 16);

    printf("After pre-whiten, block 15: ");
    vst1q_u8(temp, blocks[15]);
    hex_dump("", temp, 16);

    return 0;
}
