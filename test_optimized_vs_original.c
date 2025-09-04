/*
 * Test optimized Assembly version vs original Assembly version
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    uint8_t key_table[272];
    uint32_t key_length;
} __attribute__((aligned(16))) camellia_context;

extern int camellia_encrypt_32blks_aarch64_neon_crypto(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);
extern int camellia_encrypt_32blks_aarch64_neon_crypto_optimized(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* Anti-optimization global variables */
volatile uint32_t global_checksum = 0;

int main() {
    printf("🔧 Testing Optimized Assembly vs Original Assembly\n");
    printf("==================================================\n");
    
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 512);
    uint8_t *dst_orig = aligned_alloc(16, 512);
    uint8_t *dst_opt = aligned_alloc(16, 512);
    
    if (!ctx || !src || !dst_orig || !dst_opt) {
        printf("❌ Memory allocation failed\n");
        return 1;
    }
    
    // Initialize
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    for (int i = 0; i < 512; i++) {
        src[i] = (uint8_t)(i + 13);
    }
    
    printf("1. Correctness Test:\n");
    printf("===================\n");
    
    // Test both versions
    memset(dst_orig, 0xAA, 512);
    memset(dst_opt, 0xBB, 512);
    
    int result_orig = camellia_encrypt_32blks_aarch64_neon_crypto(ctx, dst_orig, src);
    int result_opt = camellia_encrypt_32blks_aarch64_neon_crypto_optimized(ctx, dst_opt, src);
    
    printf("Original Assembly return code: %d\n", result_orig);
    printf("Optimized Assembly return code: %d\n", result_opt);
    
    printf("Original output (first 16 bytes):  ");
    for(int i = 0; i < 16; i++) {
        printf("%02x", dst_orig[i]);
    }
    printf("\n");
    
    printf("Optimized output (first 16 bytes): ");
    for(int i = 0; i < 16; i++) {
        if(dst_orig[i] == dst_opt[i]) {
            printf("\\033[32m%02x\\033[0m", dst_opt[i]);  // Green
        } else {
            printf("\\033[31m%02x\\033[0m", dst_opt[i]);  // Red
        }
    }
    printf("\n");
    
    // Compare
    int match = memcmp(dst_orig, dst_opt, 512) == 0;
    printf("Original vs Optimized: %s\n", match ? "✅ IDENTICAL" : "❌ DIFFERENT");
    
    if (!match) {
        printf("❌ ERROR: Outputs differ, optimization broke correctness!\n");
        printf("First difference at byte: ");
        for (int i = 0; i < 512; i++) {
            if (dst_orig[i] != dst_opt[i]) {
                printf("%d (Orig=%02x, Opt=%02x)\n", i, dst_orig[i], dst_opt[i]);
                break;
            }
        }
        printf("Cannot proceed with performance testing.\n");
        return 1;
    }
    
    printf("\n2. Performance Test:\n");
    printf("===================\n");
    
    int iterations = 10000;
    
    // Test Original Assembly
    printf("🚀 Testing Original Assembly (%d iterations)...\n", iterations);
    double start_orig = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        camellia_encrypt_32blks_aarch64_neon_crypto(ctx, dst_orig, src);
        global_checksum += dst_orig[0] + dst_orig[100] + dst_orig[500];
    }
    
    double end_orig = get_time_seconds();
    double time_orig = end_orig - start_orig;
    
    // Test Optimized Assembly
    printf("⚡ Testing Optimized Assembly (%d iterations)...\n", iterations);
    double start_opt = get_time_seconds();
    
    for (int i = 0; i < iterations; i++) {
        camellia_encrypt_32blks_aarch64_neon_crypto_optimized(ctx, dst_opt, src);
        global_checksum += dst_opt[0] + dst_opt[100] + dst_opt[500];
    }
    
    double end_opt = get_time_seconds();
    double time_opt = end_opt - start_opt;
    
    // Calculate performance
    double mbps_orig = (512.0 * iterations) / (time_orig * 1024 * 1024);
    double mbps_opt = (512.0 * iterations) / (time_opt * 1024 * 1024);
    
    printf("\n📊 Performance Results:\n");
    printf("=======================\n");
    printf("Original Assembly:   %.6f seconds (%.2f MB/s)\n", time_orig, mbps_orig);
    printf("Optimized Assembly:  %.6f seconds (%.2f MB/s)\n", time_opt, mbps_opt);
    printf("Speedup:             %.3fx (%.1f%% improvement)\n", mbps_opt / mbps_orig, ((mbps_opt - mbps_orig) / mbps_orig) * 100);
    
    printf("\n🎯 Analysis:\n");
    if (mbps_opt > mbps_orig * 1.4) {
        printf("✅ EXCELLENT: Removing memory barrier achieved significant speedup!\n");
        printf("The dmb sy instruction was indeed the performance bottleneck.\n");
    } else if (mbps_opt > mbps_orig * 1.1) {
        printf("✅ GOOD: Optimization provided noticeable improvement.\n");
    } else {
        printf("⚠️  MINIMAL: Optimization provided limited improvement.\n");
        printf("There might be other performance bottlenecks.\n");
    }
    
    if (mbps_opt >= 700.0) {
        printf("🎉 SUCCESS: Optimized Assembly now matches C Intrinsics performance!\n");
    } else {
        printf("⚠️  NOTE: Performance still below C Intrinsics (~716 MB/s).\n");
    }
    
    printf("\nGlobal checksum (anti-optimization): %u\n", global_checksum);
    
    free(ctx);
    free(src);
    free(dst_orig);
    free(dst_opt);
    
    return 0;
}