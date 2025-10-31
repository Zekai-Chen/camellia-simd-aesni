/*
 * Test full Assembly byteslice_16x16b macro vs C wrapper
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void byteslice_16x16b_wrapper(uint8_t *blocks);
extern void test_byteslice_16x16b_asm(uint8_t *output, const uint8_t *input);

int main() {
    uint8_t input[256], output_c[256], output_asm[256];

    printf("Starting tests...\n");

    // Test 1: All blocks identical
    printf("Test 1: All blocks identical\n");
    const uint8_t pattern[16] = {
        0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d,
        0x2c, 0x0e, 0x68, 0x4a, 0xa4, 0x86, 0xe0, 0xc2
    };
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], pattern, 16);
    }

    memcpy(output_c, input, 256);
    memcpy(output_asm, input, 256);

    printf("  Calling C wrapper...\n");
    byteslice_16x16b_wrapper(output_c);
    printf("  C wrapper done\n");

    printf("  Calling ASM...\n");
    test_byteslice_16x16b_asm(output_asm, input);
    printf("  ASM done\n");

    int match = 1;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            printf("MISMATCH at byte %d (block %d, offset %d):\n",
                   i, i / 16, i % 16);
            printf("  C:   %02x\n", output_c[i]);
            printf("  ASM: %02x\n", output_asm[i]);
            match = 0;
            break;
        }
    }

    if (match) {
        printf("  SUCCESS: Assembly matches C wrapper\n");
        printf("  Output vector 0: ");
        for (int j = 0; j < 16; j++) printf("%02x", output_asm[j]);
        printf("\n");
        printf("  Expected:        aeaeaeaeaeaeaeaeaeaeaeaeaeaeaeae\n");
    } else {
        printf("  FAILURE: Assembly differs from C wrapper\n");
        return 1;
    }

    // Test 2: Distinctive blocks
    printf("\nTest 2: Distinctive blocks\n");
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            input[i * 16 + j] = i * 16 + j;
        }
    }

    memcpy(output_c, input, 256);
    memcpy(output_asm, input, 256);

    byteslice_16x16b_wrapper(output_c);
    test_byteslice_16x16b_asm(output_asm, input);

    match = 1;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            printf("MISMATCH at byte %d (block %d, offset %d):\n",
                   i, i / 16, i % 16);
            printf("  C:   %02x\n", output_c[i]);
            printf("  ASM: %02x\n", output_asm[i]);
            match = 0;
            break;
        }
    }

    if (match) {
        printf("  SUCCESS: Assembly matches C wrapper\n");
    } else {
        printf("  FAILURE: Assembly differs from C wrapper\n");
        return 1;
    }

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
