#include <stdio.h>
#include <stdint.h>

extern void test_double_xor_asm(uint8_t *output, const uint8_t *input, uint64_t key);

int main(void) {
    uint8_t input[256];
    uint8_t output[256];
    uint64_t key = 0x0123456789abcdefULL;

    printf("========================================\n");
    printf("Double XOR Test (XOR twice with key)\n");
    printf("========================================\n\n");

    // Fill input with pattern: (block << 4) | byte
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }

    // Call assembly function
    test_double_xor_asm(output, input, key);

    // Check results
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (input[i] != output[i]) {
            errors++;
        }
    }

    printf("Result: %d/256 bytes match\n\n", 256 - errors);

    if (errors > 0) {
        printf("First 4 blocks comparison:\n");
        for (int block = 0; block < 4; block++) {
            printf("  Block %2d:\n", block);
            printf("    Input:  ");
            for (int i = 0; i < 16; i++) {
                printf("%02x", input[block * 16 + i]);
            }
            printf("\n    Output: ");
            for (int i = 0; i < 16; i++) {
                printf("%02x", output[block * 16 + i]);
            }
            printf("\n");
        }
    }

    printf("%s\n", errors == 0 ? "✓ DOUBLE XOR TEST PASSED" : "✗ DOUBLE XOR TEST FAILED");
    return errors == 0 ? 0 : 1;
}
