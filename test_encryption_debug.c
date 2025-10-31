/*
 * Detailed encryption debug - compare C vs Assembly at each step
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
    }
    printf("\n");
}

void print_blocks(const char *label, const uint8_t *data, int num_blocks) {
    printf("%s:\n", label);
    for (int i = 0; i < num_blocks && i < 4; i++) {
        printf("  Block %2d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", data[i * 16 + j]);
        }
        printf("\n");
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256] = {0};
    uint8_t output_c[256] = {0};
    uint8_t output_asm[256] = {0};

    printf("========================================\n");
    printf("Encryption Debug Test\n");
    printf("========================================\n\n");

    // Setup key
    printf("Step 1: Key setup\n");
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    printf("  Key length: %d bits\n", ctx.key_length * 8);
    printf("  First few key_table entries:\n");
    for (int i = 0; i < 4 && i < 26; i++) {
        printf("    key_table[%2d]: %016lx\n", i, ctx.key_table[i]);
    }
    printf("\n");

    // Prepare input - use DIFFERENT blocks to see patterns
    printf("Step 2: Prepare input\n");
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
        // Add block number to make blocks unique
        input[i * 16] ^= i;
    }
    print_blocks("  Input", input, 4);
    printf("\n");

    // Encrypt with C implementation
    printf("Step 3: Encrypt with C implementation\n");
    camellia_encrypt_16blks_simd128(&ctx, output_c, input);
    print_blocks("  Output (C)", output_c, 4);
    printf("\n");

    // Encrypt with Assembly implementation
    printf("Step 4: Encrypt with Assembly implementation\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_asm, input);
    print_blocks("  Output (ASM)", output_asm, 4);
    printf("\n");

    // Compare outputs
    printf("Step 5: Compare outputs\n");
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            errors++;
        }
    }

    if (errors == 0) {
        printf("  ✓ All 256 bytes match!\n");
        return 0;
    } else {
        printf("  ✗ %d/256 bytes differ\n\n", errors);

        // Show first few differences
        printf("First 16 byte differences:\n");
        int shown = 0;
        for (int i = 0; i < 256 && shown < 16; i++) {
            if (output_c[i] != output_asm[i]) {
                printf("  Byte %3d: C=%02x ASM=%02x (diff=%02x) [block %d, offset %d]\n",
                       i, output_c[i], output_asm[i], output_c[i] ^ output_asm[i],
                       i / 16, i % 16);
                shown++;
            }
        }

        // Count errors per block
        printf("\nErrors per block:\n");
        for (int block = 0; block < 16; block++) {
            int block_errors = 0;
            for (int j = 0; j < 16; j++) {
                if (output_c[block * 16 + j] != output_asm[block * 16 + j]) {
                    block_errors++;
                }
            }
            if (block_errors > 0) {
                printf("  Block %2d: %2d/16 bytes wrong\n", block, block_errors);
            }
        }

        return 1;
    }
}
