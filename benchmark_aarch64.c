/*
 * Copyright (C) 2024 Camellia AArch64 Performance Benchmark Suite
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <sched.h>

#ifdef __aarch64__
#include <sys/auxv.h>
#include <asm/hwcap.h>
#include "camellia_aarch64_neon.h"
#include "camellia-BSD-1.2.0/camellia.h"

/* OpenSSL comparison support */
#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/camellia.h>
#endif

/* Performance measurement utilities */
typedef struct {
    struct timeval start;
    struct timeval end;
    const char *name;
    size_t data_size;
    int iterations;
} benchmark_t;

static void benchmark_start(benchmark_t *bench, const char *name, size_t data_size, int iterations)
{
    bench->name = name;
    bench->data_size = data_size;
    bench->iterations = iterations;
    gettimeofday(&bench->start, NULL);
}

static void benchmark_end(benchmark_t *bench)
{
    gettimeofday(&bench->end, NULL);
    
    double elapsed = (bench->end.tv_sec - bench->start.tv_sec) + 
                     (bench->end.tv_usec - bench->start.tv_usec) / 1000000.0;
    
    double total_bytes = (double)bench->iterations * bench->data_size;
    double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
    double throughput_gbs = throughput_mbs / 1024;
    double cycles_per_byte = (elapsed * 2800000000.0) / total_bytes; /* Assume 2.8 GHz */
    
    printf("%-30s: ", bench->name);
    printf("%.3fs, %.2f MiB/s (%.2f MB/s)", elapsed, throughput_mbs, throughput_mbs * 1.048576);
    
    if (throughput_gbs > 1.0) {
        printf(", %.2f GiB/s", throughput_gbs);
    }
    
    printf(", ~%.2f cyc/byte\n", cycles_per_byte);
}

/* CPU feature detection and reporting */
static void print_cpu_features(void)
{
    printf("=== AArch64 CPU Features ===\n");
    
    unsigned long hwcap = getauxval(AT_HWCAP);
    unsigned long hwcap2 = getauxval(AT_HWCAP2);
    
    printf("HWCAP:  0x%016lx\n", hwcap);
    printf("HWCAP2: 0x%016lx\n", hwcap2);
    printf("\nFeature Support:\n");
    
    printf("  %-20s: %s\n", "NEON (ASIMD)", (hwcap & HWCAP_ASIMD) ? "✓" : "✗");
    printf("  %-20s: %s\n", "AES", (hwcap & HWCAP_AES) ? "✓" : "✗");
    printf("  %-20s: %s\n", "SHA1", (hwcap & HWCAP_SHA1) ? "✓" : "✗");
    printf("  %-20s: %s\n", "SHA2", (hwcap & HWCAP_SHA2) ? "✓" : "✗");
    printf("  %-20s: %s\n", "CRC32", (hwcap & HWCAP_CRC32) ? "✓" : "✗");
    printf("  %-20s: %s\n", "PMULL", (hwcap & HWCAP_PMULL) ? "✓" : "✗");
    
    /* Check for ARMv8.2+ features if available */
    #ifdef HWCAP_SHA512
    printf("  %-20s: %s\n", "SHA512", (hwcap & HWCAP_SHA512) ? "✓" : "✗");
    #endif
    #ifdef HWCAP_SHA3
    printf("  %-20s: %s\n", "SHA3", (hwcap & HWCAP_SHA3) ? "✓" : "✗");
    #endif
    
    printf("\n");
}

/* Reference implementation benchmarks */
static void benchmark_reference_impl(void)
{
    printf("=== Reference Implementation Benchmarks ===\n");
    
    const uint8_t key[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    const uint8_t plaintext[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    uint8_t ciphertext[16];
    KEY_TABLE_TYPE key_table;
    benchmark_t bench;
    
    /* Key setup benchmark */
    benchmark_start(&bench, "Key Setup (128-bit)", 16, 100000);
    for (int i = 0; i < 100000; i++) {
        Camellia_Ekeygen(128, key, key_table);
    }
    benchmark_end(&bench);
    
    /* Single block encryption */
    Camellia_Ekeygen(128, key, key_table);
    benchmark_start(&bench, "Single Block Encryption", 16, 1000000);
    for (int i = 0; i < 1000000; i++) {
        Camellia_EncryptBlock(128, plaintext, key_table, ciphertext);
    }
    benchmark_end(&bench);
    
    /* Single block decryption */
    benchmark_start(&bench, "Single Block Decryption", 16, 1000000);
    for (int i = 0; i < 1000000; i++) {
        Camellia_DecryptBlock(128, ciphertext, key_table, plaintext);
    }
    benchmark_end(&bench);
    
    printf("\n");
}

/* SIMD implementation benchmarks */
static void benchmark_simd_impl(void)
{
    printf("=== NEON SIMD Implementation Benchmarks ===\n");
    
    if (!camellia_aarch64_neon_available()) {
        printf("NEON not available, skipping SIMD benchmarks\n\n");
        return;
    }
    
    const uint8_t key[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    struct camellia_simd_ctx ctx;
    uint8_t plaintext_16[16 * 16];
    uint8_t ciphertext_16[16 * 16];
    uint8_t plaintext_32[16 * 32];
    uint8_t ciphertext_32[16 * 32];
    benchmark_t bench;
    
    /* Initialize contexts */
    camellia_keysetup_neon128(&ctx, key, 128);
    
    /* Fill test data */
    for (int i = 0; i < 32; i++) {
        memcpy(plaintext_32 + i * 16, key, 16);
        if (i < 16) {
            memcpy(plaintext_16 + i * 16, key, 16);
        }
    }
    
    /* Test different S-box optimization variants */
    const char* variant_names[] = {"Baseline", "Low Register", "Latency Hiding"};
    
    for (int variant = 0; variant < 3; variant++) {
        printf("--- S-box Variant: %s ---\n", variant_names[variant]);
        camellia_set_sbox_variant((camellia_sbox_variant_t)variant);
        
        /* 16-block parallel benchmarks */
        benchmark_start(&bench, "NEON 16-Block Encrypt", 16 * 16, 50000);
        for (int i = 0; i < 50000; i++) {
            camellia_encrypt_16blks_neon128(&ctx, ciphertext_16, plaintext_16);
        }
        benchmark_end(&bench);
        
        benchmark_start(&bench, "NEON 16-Block Decrypt", 16 * 16, 50000);
        for (int i = 0; i < 50000; i++) {
            camellia_decrypt_16blks_neon128(&ctx, plaintext_16, ciphertext_16);
        }
        benchmark_end(&bench);
        
        /* 32-block parallel benchmarks */
        benchmark_start(&bench, "NEON 32-Block Encrypt", 16 * 32, 25000);
        for (int i = 0; i < 25000; i++) {
            camellia_encrypt_32blks_neon256(&ctx, ciphertext_32, plaintext_32);
        }
        benchmark_end(&bench);
        
        benchmark_start(&bench, "NEON 32-Block Decrypt", 16 * 32, 25000);
        for (int i = 0; i < 25000; i++) {
            camellia_decrypt_32blks_neon256(&ctx, plaintext_32, ciphertext_32);
        }
        benchmark_end(&bench);
        
        printf("\n");
    }
}

/* CTR mode benchmarks */
static void benchmark_ctr_mode(void)
{
    printf("=== CTR Mode Benchmarks ===\n");
    
    if (!camellia_aarch64_neon_available()) {
        printf("NEON not available, skipping CTR benchmarks\n\n");
        return;
    }
    
    const uint8_t key[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    const uint8_t iv[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    
    struct camellia_simd_ctx ctx;
    camellia_keysetup_neon128(&ctx, key, 128);
    
    /* Test different data sizes */
    const size_t test_sizes[] = {1024, 4096, 16384, 65536, 262144}; /* 1KB to 256KB */
    const int iterations[] = {10000, 5000, 1000, 200, 50};
    
    for (int i = 0; i < 5; i++) {
        size_t data_size = test_sizes[i];
        int iter = iterations[i];
        
        uint8_t *plaintext = malloc(data_size);
        uint8_t *ciphertext = malloc(data_size);
        
        if (!plaintext || !ciphertext) {
            printf("Memory allocation failed for size %zu\n", data_size);
            free(plaintext);
            free(ciphertext);
            continue;
        }
        
        /* Fill with test pattern */
        for (size_t j = 0; j < data_size; j++) {
            plaintext[j] = j & 0xff;
        }
        
        char bench_name[64];
        snprintf(bench_name, sizeof(bench_name), "CTR %zuKB", data_size / 1024);
        
        benchmark_t bench;
        benchmark_start(&bench, bench_name, data_size, iter);
        for (int j = 0; j < iter; j++) {
            camellia_ctr_encrypt_neon128(&ctx, iv, plaintext, ciphertext, data_size);
        }
        benchmark_end(&bench);
        
        free(plaintext);
        free(ciphertext);
    }
    
    printf("\n");
}

#ifdef HAVE_OPENSSL
/* OpenSSL comparison benchmarks */
static void benchmark_openssl_comparison(void)
{
    printf("=== OpenSSL Comparison Benchmarks ===\n");
    
    const uint8_t key[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    const uint8_t iv[16] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
    };
    
    const size_t data_size = 65536; /* 64KB */
    uint8_t *plaintext = malloc(data_size);
    uint8_t *ciphertext = malloc(data_size);
    
    if (!plaintext || !ciphertext) {
        printf("Memory allocation failed\n");
        free(plaintext);
        free(ciphertext);
        return;
    }
    
    /* Fill with test pattern */
    for (size_t i = 0; i < data_size; i++) {
        plaintext[i] = i & 0xff;
    }
    
    /* OpenSSL CTR mode */
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx) {
        benchmark_t bench;
        benchmark_start(&bench, "OpenSSL CTR-128", data_size, 1000);
        
        for (int i = 0; i < 1000; i++) {
            int len, final_len;
            EVP_EncryptInit_ex(ctx, EVP_camellia_128_ctr(), NULL, key, iv);
            EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, data_size);
            EVP_EncryptFinal_ex(ctx, ciphertext + len, &final_len);
        }
        
        benchmark_end(&bench);
        EVP_CIPHER_CTX_free(ctx);
    }
    
    /* Our NEON implementation */
    if (camellia_aarch64_neon_available()) {
        struct camellia_simd_ctx simd_ctx;
        camellia_keysetup_neon128(&simd_ctx, key, 128);
        
        benchmark_t bench;
        benchmark_start(&bench, "Our NEON CTR-128", data_size, 1000);
        
        for (int i = 0; i < 1000; i++) {
            camellia_ctr_encrypt_neon128(&simd_ctx, iv, plaintext, ciphertext, data_size);
        }
        
        benchmark_end(&bench);
    }
    
    free(plaintext);
    free(ciphertext);
    printf("\n");
}
#endif /* HAVE_OPENSSL */

/* Memory and cache performance analysis */
static void benchmark_memory_patterns(void)
{
    printf("=== Memory Access Pattern Analysis ===\n");
    
    if (!camellia_aarch64_neon_available()) {
        printf("NEON not available, skipping memory benchmarks\n\n");
        return;
    }
    
    const uint8_t key[16] = {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
    };
    
    struct camellia_simd_ctx ctx;
    camellia_keysetup_neon128(&ctx, key, 128);
    
    /* Test different memory layouts */
    const size_t cache_sizes[] = {32*1024, 256*1024, 2*1024*1024}; /* L1, L2, L3 typical sizes */
    const char *cache_names[] = {"L1 Cache", "L2 Cache", "L3 Cache"};
    
    for (int cache_level = 0; cache_level < 3; cache_level++) {
        size_t total_size = cache_sizes[cache_level];
        size_t blocks = total_size / 16;
        
        uint8_t *plaintext = malloc(total_size); /* Use regular malloc for compatibility */
        uint8_t *ciphertext = malloc(total_size);
        
        if (!plaintext || !ciphertext) {
            printf("Memory allocation failed for %s test\n", cache_names[cache_level]);
            free(plaintext);
            free(ciphertext);
            continue;
        }
        
        /* Fill with test pattern */
        for (size_t i = 0; i < total_size; i++) {
            plaintext[i] = i & 0xff;
        }
        
        /* Sequential access pattern */
        benchmark_t bench;
        char bench_name[64];
        snprintf(bench_name, sizeof(bench_name), "%s Sequential", cache_names[cache_level]);
        
        benchmark_start(&bench, bench_name, total_size, 100);
        for (int i = 0; i < 100; i++) {
            for (size_t j = 0; j < blocks; j += 16) {
                size_t remaining = (blocks - j < 16) ? (blocks - j) : 16;
                camellia_encrypt_16blks_neon128(&ctx, 
                                                ciphertext + j * 16, 
                                                plaintext + j * 16);
            }
        }
        benchmark_end(&bench);
        
        free(plaintext);
        free(ciphertext);
    }
    
    printf("\n");
}

/* Comprehensive system information */
static void print_system_info(void)
{
    printf("=== System Information ===\n");
    
    /* CPU information */
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0 || 
                strncmp(line, "cpu model", 9) == 0 ||
                strncmp(line, "Hardware", 8) == 0) {
                printf("%s", line);
                break;
            }
        }
        fclose(cpuinfo);
    }
    
    /* Memory information */
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                printf("%s", line);
                break;
            }
        }
        fclose(meminfo);
    }
    
    printf("\n");
    print_cpu_features();
}

int main(int argc, char *argv[])
{
    printf("Camellia AArch64 NEON Performance Benchmark Suite\n");
    printf("=================================================\n\n");
    
    /* Print system information */
    print_system_info();
    
    /* Run all benchmark suites */
    benchmark_reference_impl();
    benchmark_simd_impl();
    benchmark_ctr_mode();
    benchmark_memory_patterns();
    
#ifdef HAVE_OPENSSL
    benchmark_openssl_comparison();
#endif
    
    printf("=== Summary ===\n");
    printf("Benchmark suite completed successfully.\n");
    printf("For detailed performance analysis, use tools like:\n");
    printf("  perf stat -e cycles,instructions,cache-misses ./test_camellia_benchmark_aarch64\n");
    printf("  perf record -g ./test_camellia_benchmark_aarch64\n");
    
    return 0;
}

#else /* !__aarch64__ */

int main(void)
{
    printf("This benchmark suite requires AArch64 architecture.\n");
    return 1;
}

#endif /* __aarch64__ */