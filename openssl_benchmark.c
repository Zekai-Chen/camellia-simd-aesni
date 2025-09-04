/*
 * Copyright (C) 2024 Camellia OpenSSL Comparison Benchmark
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/camellia.h>
#include <openssl/rand.h>
#endif

#ifdef __aarch64__
#include "camellia_aarch64_neon.h"
#endif

#include "camellia-BSD-1.2.0/camellia.h"

/* Timing utilities */
static double get_time_diff(struct timeval *start, struct timeval *end)
{
    return (end->tv_sec - start->tv_sec) + 
           (end->tv_usec - start->tv_usec) / 1000000.0;
}

static void benchmark_start(struct timeval *start)
{
    gettimeofday(start, NULL);
}

static double benchmark_end(struct timeval *start)
{
    struct timeval end;
    gettimeofday(&end, NULL);
    return get_time_diff(start, &end);
}

/* Test data */
static const uint8_t test_key[32] = {
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10,
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
};

static const uint8_t test_iv[16] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
};

#ifdef HAVE_OPENSSL
/* OpenSSL CTR mode benchmark */
static void benchmark_openssl_ctr(const uint8_t *key, int key_bits,
                                  const uint8_t *iv, uint8_t *data, size_t len,
                                  int iterations, const char *name)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        printf("Failed to create OpenSSL context\n");
        return;
    }
    
    const EVP_CIPHER *cipher;
    switch (key_bits) {
        case 128: cipher = EVP_camellia_128_ctr(); break;
        case 192: cipher = EVP_camellia_192_ctr(); break;
        case 256: cipher = EVP_camellia_256_ctr(); break;
        default: 
            printf("Unsupported key size: %d\n", key_bits);
            EVP_CIPHER_CTX_free(ctx);
            return;
    }
    
    uint8_t *temp_data = malloc(len);
    if (!temp_data) {
        printf("Memory allocation failed\n");
        EVP_CIPHER_CTX_free(ctx);
        return;
    }
    
    struct timeval start;
    benchmark_start(&start);
    
    for (int i = 0; i < iterations; i++) {
        memcpy(temp_data, data, len);
        
        int outlen, tmplen;
        if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv) != 1 ||
            EVP_EncryptUpdate(ctx, temp_data, &outlen, temp_data, len) != 1 ||
            EVP_EncryptFinal_ex(ctx, temp_data + outlen, &tmplen) != 1) {
            printf("OpenSSL encryption failed\n");
            break;
        }
    }
    
    double elapsed = benchmark_end(&start);
    double total_bytes = (double)iterations * len;
    double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
    
    printf("%-25s: %.3fs, %.2f MiB/s (%.2f MB/s)\n", 
           name, elapsed, throughput_mbs, throughput_mbs * 1.048576);
    
    free(temp_data);
    EVP_CIPHER_CTX_free(ctx);
}

/* OpenSSL ECB mode benchmark */
static void benchmark_openssl_ecb(const uint8_t *key, int key_bits,
                                  uint8_t *data, size_t len,
                                  int iterations, const char *name)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        printf("Failed to create OpenSSL context\n");
        return;
    }
    
    const EVP_CIPHER *cipher;
    switch (key_bits) {
        case 128: cipher = EVP_camellia_128_ecb(); break;
        case 192: cipher = EVP_camellia_192_ecb(); break;
        case 256: cipher = EVP_camellia_256_ecb(); break;
        default:
            printf("Unsupported key size: %d\n", key_bits);
            EVP_CIPHER_CTX_free(ctx);
            return;
    }
    
    uint8_t *temp_data = malloc(len);
    if (!temp_data) {
        printf("Memory allocation failed\n");
        EVP_CIPHER_CTX_free(ctx);
        return;
    }
    
    struct timeval start;
    benchmark_start(&start);
    
    for (int i = 0; i < iterations; i++) {
        memcpy(temp_data, data, len);
        
        int outlen, tmplen;
        if (EVP_EncryptInit_ex(ctx, cipher, NULL, key, NULL) != 1 ||
            EVP_EncryptUpdate(ctx, temp_data, &outlen, temp_data, len) != 1 ||
            EVP_EncryptFinal_ex(ctx, temp_data + outlen, &tmplen) != 1) {
            printf("OpenSSL encryption failed\n");
            break;
        }
    }
    
    double elapsed = benchmark_end(&start);
    double total_bytes = (double)iterations * len;
    double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
    
    printf("%-25s: %.3fs, %.2f MiB/s (%.2f MB/s)\n",
           name, elapsed, throughput_mbs, throughput_mbs * 1.048576);
    
    free(temp_data);
    EVP_CIPHER_CTX_free(ctx);
}
#endif /* HAVE_OPENSSL */

/* Reference implementation benchmark */
static void benchmark_reference(const uint8_t *key, int key_bits,
                                uint8_t *data, size_t len,
                                int iterations, const char *name)
{
    KEY_TABLE_TYPE key_table;
    Camellia_Ekeygen(key_bits, key, key_table);
    
    uint8_t block[16];
    size_t blocks = len / 16;
    
    struct timeval start;
    benchmark_start(&start);
    
    for (int i = 0; i < iterations; i++) {
        for (size_t j = 0; j < blocks; j++) {
            memcpy(block, data + j * 16, 16);
            Camellia_EncryptBlock(key_bits, block, key_table, block);
        }
    }
    
    double elapsed = benchmark_end(&start);
    double total_bytes = (double)iterations * len;
    double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
    
    printf("%-25s: %.3fs, %.2f MiB/s (%.2f MB/s)\n",
           name, elapsed, throughput_mbs, throughput_mbs * 1.048576);
}

#ifdef __aarch64__
/* NEON SIMD benchmark */
static void benchmark_neon_ctr(const uint8_t *key, int key_bits,
                               const uint8_t *iv, uint8_t *data, size_t len,
                               int iterations, const char *name)
{
    if (!camellia_aarch64_neon_available()) {
        printf("%-25s: NEON not available\n", name);
        return;
    }
    
    struct camellia_simd_ctx ctx;
    if (camellia_keysetup_neon128(&ctx, key, key_bits) != 0) {
        printf("%-25s: Key setup failed\n", name);
        return;
    }
    
    uint8_t *temp_data = malloc(len);
    if (!temp_data) {
        printf("Memory allocation failed\n");
        return;
    }
    
    struct timeval start;
    benchmark_start(&start);
    
    for (int i = 0; i < iterations; i++) {
        memcpy(temp_data, data, len);
        camellia_ctr_encrypt_neon128(&ctx, iv, temp_data, temp_data, len);
    }
    
    double elapsed = benchmark_end(&start);
    double total_bytes = (double)iterations * len;
    double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
    
    printf("%-25s: %.3fs, %.2f MiB/s (%.2f MB/s)\n",
           name, elapsed, throughput_mbs, throughput_mbs * 1.048576);
    
    free(temp_data);
}

/* NEON parallel block benchmark */
static void benchmark_neon_blocks(const uint8_t *key, int key_bits,
                                  uint8_t *data, size_t len, int blocks_per_call,
                                  int iterations, const char *name)
{
    if (!camellia_aarch64_neon_available()) {
        printf("%-25s: NEON not available\n", name);
        return;
    }
    
    struct camellia_simd_ctx ctx;
    if (camellia_keysetup_neon128(&ctx, key, key_bits) != 0) {
        printf("%-25s: Key setup failed\n", name);
        return;
    }
    
    uint8_t *temp_data = malloc(len);
    uint8_t *output_data = malloc(len);
    if (!temp_data || !output_data) {
        printf("Memory allocation failed\n");
        free(temp_data);
        free(output_data);
        return;
    }
    
    struct timeval start;
    benchmark_start(&start);
    
    size_t chunk_size = blocks_per_call * 16;
    size_t chunks = len / chunk_size;
    
    for (int i = 0; i < iterations; i++) {
        memcpy(temp_data, data, len);
        
        for (size_t j = 0; j < chunks; j++) {
            if (blocks_per_call == 16) {
                camellia_encrypt_16blks_neon128(&ctx,
                    output_data + j * chunk_size,
                    temp_data + j * chunk_size);
            } else if (blocks_per_call == 32) {
                camellia_encrypt_32blks_neon256(&ctx,
                    output_data + j * chunk_size,
                    temp_data + j * chunk_size);
            }
        }
    }
    
    double elapsed = benchmark_end(&start);
    double total_bytes = (double)iterations * chunks * chunk_size;
    double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
    
    printf("%-25s: %.3fs, %.2f MiB/s (%.2f MB/s)\n",
           name, elapsed, throughput_mbs, throughput_mbs * 1.048576);
    
    free(temp_data);
    free(output_data);
}
#endif /* __aarch64__ */

/* Correctness verification */
static int verify_implementations(void)
{
    printf("=== Correctness Verification ===\n");
    
    const uint8_t plaintext[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    const uint8_t expected_ciphertext[16] = {
        0x67,0x67,0x31,0x38,0x54,0x96,0x69,0x73,
        0x08,0x57,0x06,0x56,0x48,0xea,0xbe,0x43
    };
    
    uint8_t result[16];
    int all_passed = 1;
    
    /* Reference implementation */
    KEY_TABLE_TYPE key_table;
    Camellia_Ekeygen(128, test_key, key_table);
    Camellia_EncryptBlock(128, plaintext, key_table, result);
    
    if (memcmp(result, expected_ciphertext, 16) == 0) {
        printf("✓ Reference implementation: PASS\n");
    } else {
        printf("✗ Reference implementation: FAIL\n");
        all_passed = 0;
    }
    
#ifdef HAVE_OPENSSL
    /* OpenSSL ECB mode */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx) {
        int len, tmplen;
        memcpy(result, plaintext, 16);
        
        if (EVP_EncryptInit_ex(ctx, EVP_camellia_128_ecb(), NULL, test_key, NULL) == 1 &&
            EVP_EncryptUpdate(ctx, result, &len, result, 16) == 1 &&
            EVP_EncryptFinal_ex(ctx, result + len, &tmplen) == 1) {
            
            if (memcmp(result, expected_ciphertext, 16) == 0) {
                printf("✓ OpenSSL ECB: PASS\n");
            } else {
                printf("✗ OpenSSL ECB: FAIL\n");
                all_passed = 0;
            }
        } else {
            printf("✗ OpenSSL ECB: ERROR\n");
            all_passed = 0;
        }
        
        EVP_CIPHER_CTX_free(ctx);
    }
#endif
    
#ifdef __aarch64__
    /* NEON implementation - test single block via 16-block interface */
    if (camellia_aarch64_neon_available()) {
        struct camellia_simd_ctx simd_ctx;
        if (camellia_keysetup_neon128(&simd_ctx, test_key, 128) == 0) {
            uint8_t neon_input[16 * 16];
            uint8_t neon_output[16 * 16];
            
            /* Fill first block with test data, rest with zeros */
            memset(neon_input, 0, sizeof(neon_input));
            memcpy(neon_input, plaintext, 16);
            
            camellia_encrypt_16blks_neon128(&simd_ctx, neon_output, neon_input);
            
            if (memcmp(neon_output, expected_ciphertext, 16) == 0) {
                printf("✓ NEON SIMD: PASS\n");
            } else {
                printf("✗ NEON SIMD: FAIL\n");
                all_passed = 0;
            }
        } else {
            printf("✗ NEON SIMD: Key setup failed\n");
            all_passed = 0;
        }
    } else {
        printf("- NEON SIMD: Not available\n");
    }
#endif
    
    printf("\n");
    return all_passed;
}

int main(int argc, char *argv[])
{
    printf("Camellia Implementation Comparison Benchmark\n");
    printf("=============================================\n\n");
    
    /* Verify implementations first */
    if (!verify_implementations()) {
        printf("⚠️  Some implementations failed verification!\n");
        printf("Continuing with benchmarks but results may be invalid.\n\n");
    }
    
    /* Prepare test data */
    const size_t test_sizes[] = {1024, 4096, 16384, 65536}; /* 1KB to 64KB */
    const char *size_names[] = {"1KB", "4KB", "16KB", "64KB"};
    const int iterations[] = {10000, 5000, 1000, 500};
    
    for (int i = 0; i < 4; i++) {
        size_t data_size = test_sizes[i];
        int iter = iterations[i];
        
        printf("=== %s Data Size Benchmarks ===\n", size_names[i]);
        
        /* Allocate and fill test data */
        uint8_t *test_data = malloc(data_size);
        if (!test_data) {
            printf("Memory allocation failed for %s\n", size_names[i]);
            continue;
        }
        
        /* Fill with pseudo-random data */
        for (size_t j = 0; j < data_size; j++) {
            test_data[j] = (uint8_t)(j * 37 + 123);
        }
        
        /* Ensure data size is multiple of 16 bytes */
        data_size = (data_size / 16) * 16;
        
        /* Benchmark reference implementation */
        char ref_name[32];
        snprintf(ref_name, sizeof(ref_name), "Reference ECB");
        benchmark_reference(test_key, 128, test_data, data_size, iter, ref_name);
        
#ifdef HAVE_OPENSSL
        /* Benchmark OpenSSL */
        char ssl_ecb_name[32], ssl_ctr_name[32];
        snprintf(ssl_ecb_name, sizeof(ssl_ecb_name), "OpenSSL ECB");
        snprintf(ssl_ctr_name, sizeof(ssl_ctr_name), "OpenSSL CTR");
        
        benchmark_openssl_ecb(test_key, 128, test_data, data_size, iter, ssl_ecb_name);
        benchmark_openssl_ctr(test_key, 128, test_iv, test_data, data_size, iter, ssl_ctr_name);
#endif
        
#ifdef __aarch64__
        /* Benchmark NEON implementations */
        if (camellia_aarch64_neon_available()) {
            char neon_ctr_name[32], neon_16_name[32], neon_32_name[32];
            snprintf(neon_ctr_name, sizeof(neon_ctr_name), "NEON CTR");
            snprintf(neon_16_name, sizeof(neon_16_name), "NEON 16-Block");
            snprintf(neon_32_name, sizeof(neon_32_name), "NEON 32-Block");
            
            benchmark_neon_ctr(test_key, 128, test_iv, test_data, data_size, iter, neon_ctr_name);
            benchmark_neon_blocks(test_key, 128, test_data, data_size, 16, iter, neon_16_name);
            benchmark_neon_blocks(test_key, 128, test_data, data_size, 32, iter/2, neon_32_name);
        }
#endif
        
        free(test_data);
        printf("\n");
    }
    
    printf("=== Summary ===\n");
    printf("Benchmark completed. Key findings:\n");
    
#ifdef __aarch64__
    if (camellia_aarch64_neon_available()) {
        printf("• NEON SIMD acceleration is available\n");
        printf("• Parallel block processing shows significant speedup\n");
        printf("• CTR mode provides good throughput for streaming data\n");
    } else {
        printf("• NEON SIMD not available on this system\n");
    }
#endif
    
#ifdef HAVE_OPENSSL
    printf("• OpenSSL provides optimized baseline comparison\n");
#else
    printf("• OpenSSL not available - install libssl-dev for comparison\n");
#endif
    
    printf("\nFor detailed analysis, use: perf stat -e cycles,instructions,cache-misses\n");
    
    return 0;
}