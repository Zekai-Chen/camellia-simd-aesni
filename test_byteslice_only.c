/*
 * Test byteslice transformation only
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

extern void test_byteslice_16x16b_asm(uint8_t *output, const uint8_t *input);

int main() {
    uint8_t input[256], output[256];

    // Initialize input: all 16 blocks identical
    const uint8_t pattern[16] = {
        0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d,
        0x2c, 0x0e, 0x68, 0x4a, 0xa4, 0x86, 0xe0, 0xc2
    };
    
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], pattern, 16);
    }

    printf("Input (all blocks identical):\n");
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", input[i]);
    printf("\n");

    // Call Assembly byteslice
    test_byteslice_16x16b_asm(output, input);

    printf("\nOutput after byteslice:\n");
    for (int i = 0; i < 16; i++) {
        printf("  Vector %2d: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", output[i * 16 + j]);
        printf("\n");
    }

    // Check expected values
    printf("\nExpected (each vector should have all bytes identical):\n");
    printf("  Vector  0: aeaeaeaeaeaeaeaeaeaeaeaeaeaeaeae\n");
    printf("  Vector  1: 71717171717171717171717171717171\n");
    printf("  Vector  8: 2c2c2c2c2c2c2c2c2c2c2c2c2c2c2c2c\n");
    printf("  Vector  9: 0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e\n");

    // Verify
    int errors = 0;
    const uint8_t expected_bytes[] = {
        0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d,
        0x2c, 0x0e, 0x68, 0x4a, 0xa4, 0x86, 0xe0, 0xc2
    };

    for (int vec = 0; vec < 16; vec++) {
        uint8_t expected_byte = expected_bytes[vec];
        for (int pos = 0; pos < 16; pos++) {
            if (output[vec * 16 + pos] != expected_byte) {
                if (errors == 0) {
                    printf("\n*** ERRORS FOUND ***\n");
                }
                printf("Vector %2d, byte %2d: got %02x, expected %02x\n",
                       vec, pos, output[vec * 16 + pos], expected_byte);
                errors++;
            }
        }
    }

    if (errors == 0) {
        printf("\n*** SUCCESS: Byteslice is correct! ***\n");
        return 0;
    } else {
        printf("\n*** FAILURE: %d errors in byteslice ***\n", errors);
        return 1;
    }
}
