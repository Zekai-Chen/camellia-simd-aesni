/*
 * Debug version - prints intermediate states from assembly
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// External debug function we'll create
extern void camellia_encrypt_debug_asm(struct camellia_simd_ctx *ctx, void *out, const void *in,
                                       uint8_t *ab_after_byteslice, uint8_t *cd_after_byteslice);

// Test key and data
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_state(const char *label, const uint8_t *data, int count) {
    printf("%s:\n", label);
    for (int i = 0; i < count; i++) {
        printf("  v%-2d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x ", data[i * 16 + j]);
        }
        printf("\n");
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t plaintext[256];
    uint8_t ciphertext[256];
    uint8_t ab_state[128];  // 8 vectors
    uint8_t cd_state[128];  // 8 vectors

    printf("Assembly Debug Output\n");
    printf("=====================\n\n");

    // Key setup
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    printf("Key table[0]: 0x%016lx\n", ctx.key_table[0]);
    printf("Key table[2]: 0x%016lx\n\n", ctx.key_table[2]);

    // Prepare plaintext
    for (int i = 0; i < 16; i++) {
        memcpy(plaintext + i * 16, test_plaintext, 16);
    }

    // Call debug version
    camellia_encrypt_debug_asm(&ctx, ciphertext, plaintext, ab_state, cd_state);

    printf("AB state after byteslice (first 8 vectors):\n");
    print_state("", ab_state, 8);

    printf("CD state after byteslice (second 8 vectors):\n");
    print_state("", cd_state, 8);

    printf("Output (first 2 blocks):\n");
    for (int i = 0; i < 2; i++) {
        printf("  Block %d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x ", ciphertext[i * 16 + j]);
        }
        printf("\n");
    }

    return 0;
}
