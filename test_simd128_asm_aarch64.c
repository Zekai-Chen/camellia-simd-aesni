/*
 * test_simd128_asm_aarch64.c - Test program for AArch64 Assembly implementation
 *
 * Copyright (C) 2024 Optimized Camellia SIMD Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "camellia_simd.h"

/* Assembly implementations - external symbols */
extern int camellia_keysetup_simd128_aarch64_asm(struct camellia_simd_ctx *ctx, 
                                                  const void *key, unsigned int keylen);
extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx, 
                                                        void *out, const void *in);
extern void camellia_decrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                        void *out, const void *in);

/* Test vectors from RFC 3713 */
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

/* Convert bytes to hex string */
static void bytes_to_hex(const uint8_t *bytes, char *hex, int len) {
    for (int i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
}

/* Test single block encryption for correctness */
static int test_single_block(void) {
    struct camellia_simd_ctx ctx;
    uint8_t output[16];
    char hex[33] = {0};
    
    printf("Testing Assembly implementation (single block):\n");
    
    /* Setup key */
    if (camellia_keysetup_simd128_aarch64_asm(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    
    /* Create 16 blocks with the test vector in the first position */
    uint8_t input_16blks[16 * 16] = {0};
    uint8_t output_16blks[16 * 16] = {0};
    memcpy(input_16blks, test_plaintext, 16);
    
    /* Encrypt */
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output_16blks, input_16blks);
    
    /* Extract first block */
    memcpy(output, output_16blks, 16);
    
    /* Check result */
    bytes_to_hex(output, hex, 16);
    printf("  Plaintext:  ");
    for (int i = 0; i < 16; i++) printf("%02x", test_plaintext[i]);
    printf("\n");
    printf("  Ciphertext: %s\n", hex);
    printf("  Expected:   ");
    for (int i = 0; i < 16; i++) printf("%02x", test_ciphertext_128[i]);
    printf("\n");
    
    if (memcmp(output, test_ciphertext_128, 16) == 0) {
        printf("  ✓ Test PASSED\n");
        return 0;
    } else {
        printf("  ✗ Test FAILED\n");
        return 1;
    }
}

/* Test 16-block parallel processing */
static int test_16block_parallel(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[16 * 16];
    uint8_t output[16 * 16];
    uint8_t expected[16 * 16];
    
    printf("\nTesting 16-block parallel processing:\n");
    
    /* Setup key */
    if (camellia_keysetup_simd128_aarch64_asm(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }
    
    /* Create 16 different input blocks */
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            input[i * 16 + j] = (i + j) & 0xFF;
        }
    }
    
    /* Encrypt with assembly version */
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);
    
    /* Encrypt with reference to get expected output */
    camellia_encrypt_16blks_simd128(&ctx, expected, input);
    
    /* Compare results */
    if (memcmp(output, expected, 16 * 16) == 0) {
        printf("  ✓ 16-block parallel test PASSED\n");
        return 0;
    } else {
        printf("  ✗ 16-block parallel test FAILED\n");
        printf("  First block mismatch at byte:\n");
        for (int i = 0; i < 16; i++) {
            if (output[i] != expected[i]) {
                printf("    Position %d: got %02x, expected %02x\n", 
                       i, output[i], expected[i]);
                break;
            }
        }
        return 1;
    }
}

/* Benchmark performance */
static void benchmark_performance(void) {
    struct camellia_simd_ctx ctx;
    uint8_t *input, *output;
    const int num_blocks = 1024 * 64;  /* 1 MB of data */
    const int block_size = 16;
    const int iterations = 100;
    
    printf("\nPerformance Benchmark:\n");
    printf("  Data size: %d KB per iteration\n", (num_blocks * block_size) / 1024);
    printf("  Iterations: %d\n", iterations);
    
    /* Allocate aligned buffers */
    input = aligned_alloc(16, num_blocks * block_size);
    output = aligned_alloc(16, num_blocks * block_size);
    
    if (!input || !output) {
        printf("ERROR: Memory allocation failed\n");
        goto cleanup;
    }
    
    /* Initialize input data */
    for (int i = 0; i < num_blocks * block_size; i++) {
        input[i] = i & 0xFF;
    }
    
    /* Setup key */
    camellia_keysetup_simd128_aarch64_asm(&ctx, test_key_128, 16);
    
    /* Warmup */
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < num_blocks; j += 16) {
            camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, 
                                                        output + j * block_size,
                                                        input + j * block_size);
        }
    }
    
    /* Benchmark Assembly implementation */
    clock_t start = clock();
    for (int iter = 0; iter < iterations; iter++) {
        for (int j = 0; j < num_blocks; j += 16) {
            camellia_encrypt_16blks_simd128_aarch64_asm(&ctx,
                                                        output + j * block_size,
                                                        input + j * block_size);
        }
    }
    clock_t end = clock();
    
    double asm_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double asm_throughput = (iterations * num_blocks * block_size) / (1024.0 * 1024.0 * asm_time);
    
    /* Benchmark C/intrinsics implementation for comparison */
    start = clock();
    for (int iter = 0; iter < iterations; iter++) {
        for (int j = 0; j < num_blocks; j += 16) {
            camellia_encrypt_16blks_simd128(&ctx,
                                           output + j * block_size,
                                           input + j * block_size);
        }
    }
    end = clock();
    
    double c_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    double c_throughput = (iterations * num_blocks * block_size) / (1024.0 * 1024.0 * c_time);
    
    printf("\nResults:\n");
    printf("  Assembly version:    %.2f MiB/s (%.3f seconds)\n", asm_throughput, asm_time);
    printf("  C/intrinsics version: %.2f MiB/s (%.3f seconds)\n", c_throughput, c_time);
    printf("  Speedup: %.2fx\n", asm_throughput / c_throughput);
    
cleanup:
    if (input) free(input);
    if (output) free(output);
}

/* Test decryption functionality */
static int test_decryption(void) {
    struct camellia_simd_ctx ctx;
    uint8_t plaintext[16 * 16] = {0};
    uint8_t ciphertext[16 * 16] = {0};
    uint8_t decrypted[16 * 16] = {0};
    
    printf("\nTesting decryption:\n");
    
    /* Setup key */
    camellia_keysetup_simd128_aarch64_asm(&ctx, test_key_128, 16);
    
    /* Create test data */
    for (int i = 0; i < 16 * 16; i++) {
        plaintext[i] = i & 0xFF;
    }
    
    /* Encrypt */
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, ciphertext, plaintext);
    
    /* Decrypt */
    camellia_decrypt_16blks_simd128_aarch64_asm(&ctx, decrypted, ciphertext);
    
    /* Verify */
    if (memcmp(plaintext, decrypted, 16 * 16) == 0) {
        printf("  ✓ Decryption test PASSED\n");
        return 0;
    } else {
        printf("  ✗ Decryption test FAILED\n");
        return 1;
    }
}

int main(int argc, char *argv[]) {
    int errors = 0;
    
    printf("========================================\n");
    printf("Camellia AArch64 Assembly Implementation Test\n");
    printf("========================================\n\n");
    
    /* Run correctness tests */
    errors += test_single_block();
    errors += test_16block_parallel();
    errors += test_decryption();
    
    /* Run performance benchmark */
    benchmark_performance();
    
    /* Summary */
    printf("\n========================================\n");
    if (errors == 0) {
        printf("✓ All tests PASSED\n");
    } else {
        printf("✗ %d test(s) FAILED\n", errors);
    }
    printf("========================================\n");
    
    return errors;
}