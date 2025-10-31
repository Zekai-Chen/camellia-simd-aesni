/*
 * Debug: Print intermediate values during byteslice + prewhiten
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Test plaintext - 16 identical blocks
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Test key
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// External assembly - just byteslice, no prewhiten
extern void test_byteslice_asm(uint8_t *output, const uint8_t *input);

static void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%2d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[idx * 16 + i]);
    }
    printf("\n");
}

int main(void) {
    uint8_t input[256];
    uint8_t bytesliced[256];

    printf("========================================\n");
    printf("Debug: Byteslice Only (No Prewhiten)\n");
    printf("========================================\n\n");

    // Prepare input - 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    printf("Input (first block):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Apply byteslice only
    test_byteslice_asm(bytesliced, input);

    printf("After byteslice (should match our previous 100%% correct test):\n");
    printf("AB vectors (bytes 0-7):\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, bytesliced);
    }
    printf("\nCD vectors (bytes 8-15):\n");
    for (int i = 8; i < 16; i++) {
        print_vector("  ", i, bytesliced);
    }

    // Verify against expected
    uint8_t expected_ab[8] = {0x01, 0x45, 0x23, 0x67, 0xfe, 0xba, 0xdc, 0x98};
    uint8_t expected_cd[8] = {0x89, 0xcd, 0xab, 0xef, 0x76, 0x32, 0x54, 0x10};

    printf("\nExpected values:\n");
    printf("AB: ");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", expected_ab[i]);
    }
    printf("\nCD: ");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", expected_cd[i]);
    }
    printf("\n\nActual first byte of each vector:\n");
    printf("AB: ");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", bytesliced[i * 16]);
    }
    printf("\nCD: ");
    for (int i = 8; i < 16; i++) {
        printf("%02x ", bytesliced[i * 16]);
    }
    printf("\n");

    return 0;
}
