/*
 * Simple test for input loading and whitening (no byteslice)
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Test wrapper that just loads and XORs input
extern void test_simple_whitening_asm(uint8_t *output, const uint8_t *input, uint64_t key);

int main(void) {
    uint8_t input[256];
    uint8_t output[256];
    uint64_t key = 0xd20d72f2af5286b2ULL;

    printf("Simple whitening test\n");
    printf("Input[0]: %02x, Key: %016lx\n", input[0], key);

    // Initialize input
    for (int i = 0; i < 256; i++) {
        input[i] = i;
    }

    test_simple_whitening_asm(output, input, key);

    printf("Output[0]: %02x (expected: %02x)\n", output[0], (uint8_t)(0 ^ 0xb2));
    printf("Output[1]: %02x (expected: %02x)\n", output[1], (uint8_t)(1 ^ 0x86));

    return 0;
}
