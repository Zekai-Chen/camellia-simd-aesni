/*
 * Isolated byteslice test - compare Assembly vs C byteslice only
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// Test input: 16 identical blocks for easy pattern recognition
static uint8_t test_input[256];

extern uint8_t debug_ab_before_first_round[128];
extern uint8_t debug_cd_after_byteslice[128];

extern void test_byteslice_asm(uint8_t *output, const uint8_t *input);

// C reference byteslice (copied from camellia_simd128_with_aes_instruction_set.c)
void byteslice_c_reference(uint8_t *output, const uint8_t *input) {
    // This will call the C inpack16_post which includes byteslice
    struct camellia_simd_ctx ctx;
    uint8_t dummy_key[16] = {0};

    camellia_keysetup_simd128(&ctx, dummy_key, 16);

    // We'll implement inline later...
}

void print_vector(const char *label, const uint8_t *data, int idx) {
    printf("%s[%d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[idx * 16 + i]);
    }
    printf("\n");
}

int main(void) {
    // Initialize: 16 identical blocks, each containing 00 01 02 ... 0f
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            test_input[i * 16 + j] = j;
        }
    }

    printf("========================================\n");
    printf("Byteslice Isolated Test\n");
    printf("========================================\n\n");

    printf("Input: 16 identical blocks, each = 00 01 02 03 ... 0f\n\n");

    // For now, just call the assembly and check CD values
    struct camellia_simd_ctx ctx;
    uint8_t dummy_key[16] = {0};
    uint8_t output[256];

    camellia_keysetup_simd128(&ctx, dummy_key, 16);

    // Call the assembly encryption (which includes byteslice)
    extern void camellia_encrypt_16blks_simd128_aarch64_asm(
        struct camellia_simd_ctx *ctx,
        void *out, const void *in
    );

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, test_input);

    printf("Assembly CD after byteslice:\n");
    for (int i = 0; i < 8; i++) {
        print_vector("  CD", debug_cd_after_byteslice, i);
    }

    printf("\nChecking for corruption (AB values in CD):\n");
    printf("  AB[4] = 5b5b5b5b...\n");
    printf("  AB[5] = a6a6a6a6...\n");
    printf("  AB[6] = bfbfbfbf...\n");
    printf("  AB[7] = 1d1d1d1d...\n");

    // Check if CD contains these AB patterns
    int corrupted = 0;
    for (int i = 0; i < 128; i++) {
        uint8_t val = debug_cd_after_byteslice[i];
        if (val == 0x5b || val == 0xa6 || val == 0xbf || val == 0x1d) {
            printf("  CORRUPTION at CD byte %d: value = 0x%02x\n", i, val);
            corrupted = 1;
        }
    }

    if (corrupted) {
        printf("\n=> CD is CORRUPTED with AB values!\n");
        return 1;
    } else {
        printf("\n=> CD looks OK (no obvious AB contamination)\n");
        return 0;
    }
}
