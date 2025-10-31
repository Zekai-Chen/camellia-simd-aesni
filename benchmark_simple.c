/*
 * Simple benchmark to compare C intrinsics vs our assembly implementation
 *
 * This measures the actual performance of:
 * 1. Pure C intrinsics (Kivilinna's implementation)
 * 2. Our AArch64 assembly macros
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "camellia_simd.h"

// Test key and data
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Timing helper
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Benchmark function
static void benchmark_implementation(
    const char *name,
    void (*encrypt_func)(struct camellia_simd_ctx *, void *, const void *),
    int blocks_per_call)
{
    struct camellia_simd_ctx ctx;
    uint8_t *plaintext;
    uint8_t *ciphertext;
    int iterations;
    double start_time, end_time, elapsed;
    uint64_t total_bytes;
    double throughput_mbps;

    // Allocate buffers
    size_t buffer_size = blocks_per_call * 16;
    plaintext = aligned_alloc(64, buffer_size);
    ciphertext = aligned_alloc(64, buffer_size);

    if (!plaintext || !ciphertext) {
        printf("Memory allocation failed\n");
        return;
    }

    // Initialize key (keylen is in bytes, not bits!)
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("Key setup failed\n");
        free(plaintext);
        free(ciphertext);
        return;
    }

    // Fill plaintext with test data
    for (size_t i = 0; i < buffer_size; i++) {
        plaintext[i] = (uint8_t)(i & 0xff);
    }

    // Warmup
    for (int i = 0; i < 100; i++) {
        encrypt_func(&ctx, ciphertext, plaintext);
    }

    // Determine iterations based on block count
    if (blocks_per_call == 16) {
        iterations = 100000;  // More iterations for 16-block version
    } else {
        iterations = 50000;   // Fewer for 32-block version
    }

    // Benchmark
    printf("Testing %s (%d blocks per call)...\n", name, blocks_per_call);
    printf("  Running %d iterations...\n", iterations);

    start_time = get_time_ms();

    for (int i = 0; i < iterations; i++) {
        encrypt_func(&ctx, ciphertext, plaintext);
    }

    end_time = get_time_ms();
    elapsed = (end_time - start_time) / 1000.0;  // Convert to seconds

    // Calculate throughput
    total_bytes = (uint64_t)iterations * blocks_per_call * 16;
    throughput_mbps = (total_bytes / (1024.0 * 1024.0)) / elapsed;

    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Data: %.2f MiB\n", total_bytes / (1024.0 * 1024.0));
    printf("  Throughput: %.2f MiB/s\n", throughput_mbps);
    printf("  Blocks/sec: %.2f million\n", (iterations * blocks_per_call) / (elapsed * 1000000.0));
    printf("\n");

    free(plaintext);
    free(ciphertext);
}

int main(void) {
    printf("========================================\n");
    printf("Camellia SIMD Benchmark\n");
    printf("========================================\n\n");

    printf("Platform: AArch64 with NEON Crypto Extensions\n");
    printf("Algorithm: Camellia-128 (18 rounds)\n");
    printf("Method: Byte-slicing with AES-NI acceleration\n\n");

    // Test 16-block implementations
    printf("----------------------------------------\n");
    printf("16-Block Implementations (256 bytes)\n");
    printf("----------------------------------------\n\n");

    // C intrinsics implementation (reference)
    benchmark_implementation(
        "C Intrinsics (Kivilinna)",
        camellia_encrypt_16blks_simd128,
        16
    );

    // Our assembly implementation
    benchmark_implementation(
        "AArch64 Assembly (Ours)",
        camellia_encrypt_16blks_simd128_aarch64_asm,
        16
    );

    printf("========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");

    return 0;
}
