/*
 * Test byteslice roundtrip: byteslice -> unbyteslice should return original data
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern void test_byteslice_roundtrip_asm(uint8_t *output, const uint8_t *input);

int main(void) {
    uint8_t input[256];
    uint8_t output[256];

    printf("========================================\n");
    printf("Byteslice Roundtrip Test\n");
    printf("========================================\n\n");

    // Fill input with distinct pattern for each block
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }

    printf("Input (first 4 blocks):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x", input[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    // Test roundtrip
    test_byteslice_roundtrip_asm(output, input);

    printf("Output (first 4 blocks):\n");
    for (int block = 0; block < 4; block++) {
        printf("  Block %2d: ", block);
        for (int i = 0; i < 16; i++) {
            printf("%02x", output[block * 16 + i]);
        }
        printf("\n");
    }
    printf("\n");

    // Compare
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (input[i] != output[i]) {
            errors++;
        }
    }

    printf("Result: %d/256 bytes match\n", 256 - errors);

    if (errors > 0) {
        printf("\nFirst 10 errors:\n");
        int count = 0;
        for (int i = 0; i < 256 && count < 10; i++) {
            if (input[i] != output[i]) {
                printf("  Byte %3d (block %2d, offset %2d): in=%02x out=%02x\n",
                       i, i/16, i%16, input[i], output[i]);
                count++;
            }
        }
    }

    printf("\n%s\n", errors == 0 ? "✓ ROUNDTRIP SUCCESSFUL" : "✗ ROUNDTRIP FAILED");

    return errors == 0 ? 0 : 1;
}
