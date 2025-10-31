/*
 * Minimal encryption test - test single block encryption step by step
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// Test vectors from RFC 3713
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_ciphertext_128[16] = {
    0x67, 0x67, 0x31, 0x38, 0x54, 0x96, 0x69, 0x73,
    0x08, 0x57, 0x06, 0x56, 0x48, 0xea, 0xbe, 0x43
};

// External functions
extern void camellia_encrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
                                             void *out, const void *in);
extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                          void *out, const void *in);
extern int camellia_keysetup_simd128(struct camellia_simd_ctx *ctx,
                                      const void *key, unsigned int keylen);

void print_hex(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0 && i < len - 1) printf("\n%*s  ", (int)strlen(label), "");
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256] = {0};
    uint8_t output_c[256] = {0};
    uint8_t output_asm[256] = {0};

    printf("========================================\n");
    printf("Minimal Encryption Test\n");
    printf("========================================\n\n");

    // Setup key
    printf("Step 1: Key setup\n");
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    printf("  Key length: %d bits\n", ctx.key_length * 8);
    printf("  Key table[0]: %016lx\n", ctx.key_table[0]);
    printf("  Key table[1]: %016lx\n", ctx.key_table[1]);
    printf("\n");

    // Prepare input (copy test plaintext to all 16 blocks)
    printf("Step 2: Prepare input (16 identical blocks)\n");
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }
    print_hex("  Input block 0", &input[0], 16);
    printf("\n");

    // Encrypt with C implementation
    printf("Step 3: Encrypt with C/intrinsics implementation\n");
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);
    print_hex("  Output block 0 (C)", &output_c[0], 16);
    print_hex("  Expected", test_ciphertext_128, 16);

    if (memcmp(&output_c[0], test_ciphertext_128, 16) == 0) {
        printf("  ✓ C implementation correct\n");
    } else {
        printf("  ✗ C implementation FAILED\n");
        return 1;
    }
    printf("\n");

    // Encrypt with Assembly implementation
    printf("Step 4: Encrypt with Assembly implementation\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);
    print_hex("  Output block 0 (ASM)", &output_asm[0], 16);
    print_hex("  Expected", test_ciphertext_128, 16);

    if (memcmp(&output_asm[0], test_ciphertext_128, 16) == 0) {
        printf("  ✓ Assembly implementation correct\n");
    } else {
        printf("  ✗ Assembly implementation FAILED\n");

        // Show differences
        printf("\n  Byte-by-byte comparison:\n");
        for (int i = 0; i < 16; i++) {
            if (output_asm[i] != test_ciphertext_128[i]) {
                printf("    Byte %2d: got %02x, expected %02x (diff: %02x)\n",
                       i, output_asm[i], test_ciphertext_128[i],
                       output_asm[i] ^ test_ciphertext_128[i]);
            }
        }
    }
    printf("\n");

    // Compare all 16 blocks
    printf("Step 5: Verify all 16 blocks\n");
    int errors = 0;
    for (int block = 0; block < 16; block++) {
        if (memcmp(&output_asm[block * 16], test_ciphertext_128, 16) != 0) {
            errors++;
            if (errors <= 3) {
                printf("  Block %2d mismatch:\n", block);
                print_hex("    Got", &output_asm[block * 16], 16);
            }
        }
    }

    if (errors == 0) {
        printf("  ✓ All 16 blocks correct\n");
    } else {
        printf("  ✗ %d/16 blocks failed\n", errors);
    }

    printf("\n========================================\n");
    printf("%s\n", errors == 0 ? "✓ TEST PASSED" : "✗ TEST FAILED");
    printf("========================================\n");

    return errors == 0 ? 0 : 1;
}
