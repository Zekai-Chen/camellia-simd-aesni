/*
 * Test to compare C and assembly at the output stage
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                          void *out, const void *in);

void print_block(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output_c[256], output_asm[256];

    printf("========================================\n");
    printf("Output Stage Comparison Test\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Create input: repeat test block 16 times
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("Running C implementation...\n");
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);

    printf("\nRunning assembly implementation...\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);

    printf("\n========================================\n");
    printf("Output Comparison\n");
    printf("========================================\n\n");

    print_block("C   block 0", output_c);
    print_block("ASM block 0", output_asm);
    printf("\n");
    print_block("C   block 1", output_c + 16);
    print_block("ASM block 1", output_asm + 16);
    printf("\n");
    print_block("C   block 15", output_c + 15*16);
    print_block("ASM block 15", output_asm + 15*16);

    int match = (memcmp(output_c, output_asm, 256) == 0);
    printf("\nResult: %s\n", match ? "PASS" : "FAIL");

    if (!match) {
        printf("\nFirst difference at byte: ");
        for (int i = 0; i < 256; i++) {
            if (output_c[i] != output_asm[i]) {
                printf("%d (C=%02x, ASM=%02x)\n", i, output_c[i], output_asm[i]);
                break;
            }
        }
    }

    return match ? 0 : 1;
}
