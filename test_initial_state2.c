/*
 * Test with simpler approach - manually do prewhiten in C after assembly byteslice
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// Test plaintext
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Test key
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// External assembly - just byteslice
extern void test_byteslice_asm(uint8_t *output, const uint8_t *input);

static void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%2d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", data[idx * 16 + i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t bytesliced[256];
    uint8_t ab[128], cd[128];

    printf("========================================\n");
    printf("Manual Prewhiten Test\n");
    printf("========================================\n\n");

    // Setup key
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }

    printf("Key schedule:\n");
    printf("  key_table[0]: 0x%016lx\n", ctx.key_table[0]);
    printf("  key_table[1]: 0x%016lx\n\n", ctx.key_table[1]);

    // Prepare input - 16 identical blocks
    for (int i = 0; i < 16; i++) {
        memcpy(input + i * 16, test_plaintext, 16);
    }

    // Apply byteslice
    test_byteslice_asm(bytesliced, input);

    // Split into AB and CD
    memcpy(ab, bytesliced, 128);       // Vectors 0-7
    memcpy(cd, bytesliced + 128, 128);  // Vectors 8-15

    printf("After byteslice (before prewhiten):\n");
    printf("AB[0-3]: ");
    for (int i = 0; i < 4; i++) {
        printf("%02x ", ab[i * 16]);
    }
    printf("\nCD[0-3]: ");
    for (int i = 0; i < 4; i++) {
        printf("%02x ", cd[i * 16]);
    }
    printf("\n\n");

    // Manual prewhiten: AB ^= key_table[0], CD ^= key_table[1]
    uint64_t kw1 = ctx.key_table[0];
    uint64_t kw2 = ctx.key_table[1];

    printf("Prewhiten keys:\n");
    printf("  kw1 = 0x%016lx\n", kw1);
    printf("  kw2 = 0x%016lx\n\n", kw2);

    // Apply prewhiten to each vector
    for (int vec = 0; vec < 8; vec++) {
        for (int byte = 0; byte < 16; byte++) {
            // Each byte XORs with corresponding byte of kw1/kw2 (replicated)
            uint8_t key_byte = (kw1 >> ((byte % 8) * 8)) & 0xff;
            ab[vec * 16 + byte] ^= key_byte;
        }
    }

    for (int vec = 0; vec < 8; vec++) {
        for (int byte = 0; byte < 16; byte++) {
            uint8_t key_byte = (kw2 >> ((byte % 8) * 8)) & 0xff;
            cd[vec * 16 + byte] ^= key_byte;
        }
    }

    printf("After manual prewhiten:\n");
    printf("AB vectors:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, ab);
    }
    printf("\nCD vectors:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  ", i, cd);
    }

    return 0;
}
