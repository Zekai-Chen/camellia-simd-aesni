#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    uint8_t key_table[272];
    uint32_t key_length;
} __attribute__((aligned(16))) camellia_context;

extern int camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);
extern int camellia_encrypt_32blks_aarch64_neon_crypto_simple_extreme(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

volatile uint32_t global_checksum = 0;

int main() {
    printf("🏆 Extreme Assembly Optimization Test\n");
    printf("====================================\n");
    printf("Target: >750 MB/s (beat compiler optimization)\n\n");
    
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 512);
    uint8_t *reference_output = aligned_alloc(16, 512);
    uint8_t *extreme_output = aligned_alloc(16, 512);
    
    // Initialize
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    for (int i = 0; i < 512; i++) {
        src[i] = (uint8_t)(i + 13);
    }
    
    // Test correctness
    printf("1. Correctness Test:\n");
    printf("===================\n");
    
    camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(ctx, reference_output, src);
    int result = camellia_encrypt_32blks_aarch64_neon_crypto_simple_extreme(ctx, extreme_output, src);
    
    if (result != 0) {
        printf("❌ Extreme version returned error: %d\n", result);
        return 1;
    }
    
    int match = memcmp(reference_output, extreme_output, 512) == 0;
    if (match) {
        printf("✅ Extreme Assembly: Output matches reference\n\n");
    } else {
        printf("❌ Extreme Assembly: Output differs from reference\n");
        for (int i = 0; i < 512; i++) {
            if (reference_output[i] != extreme_output[i]) {
                printf("   First difference at byte %d: expected %02x, got %02x\n", 
                       i, reference_output[i], extreme_output[i]);
                break;
            }
        }
        return 1;
    }
    
    // Performance test
    printf("2. Performance Test:\n");
    printf("===================\n");
    
    int iterations = 10000;
    
    // Reference (C Intrinsics)
    printf("Testing C Intrinsics reference...\n");
    double start = get_time_seconds();
    for (int i = 0; i < iterations; i++) {
        camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(ctx, reference_output, src);
        global_checksum += reference_output[0] + reference_output[256] + reference_output[511];
    }
    double end = get_time_seconds();
    double ref_time = end - start;
    double ref_mbps = (512.0 * iterations) / (ref_time * 1024 * 1024);
    
    printf("   C Intrinsics: %.2f MB/s\n\n", ref_mbps);
    
    // Extreme Assembly
    printf("Testing Extreme Assembly...\n");
    start = get_time_seconds();
    for (int i = 0; i < iterations; i++) {
        camellia_encrypt_32blks_aarch64_neon_crypto_simple_extreme(ctx, extreme_output, src);
        global_checksum += extreme_output[0] + extreme_output[256] + extreme_output[511];
    }
    end = get_time_seconds();
    double extreme_time = end - start;
    double extreme_mbps = (512.0 * iterations) / (extreme_time * 1024 * 1024);
    
    printf("   Extreme Assembly: %.2f MB/s\n\n", extreme_mbps);
    
    // Results
    printf("3. Results:\n");
    printf("===========\n");
    printf("C Intrinsics:     %.1f MB/s (reference)\n", ref_mbps);
    printf("Extreme Assembly: %.1f MB/s\n", extreme_mbps);
    
    double improvement = (extreme_mbps / ref_mbps - 1.0) * 100.0;
    if (extreme_mbps > ref_mbps) {
        printf("Improvement:      +%.1f%% 🎉\n", improvement);
        if (extreme_mbps > 750.0) {
            printf("🏆 VICTORY: Achieved >750 MB/s target!\n");
        } else if (extreme_mbps > ref_mbps * 1.05) {
            printf("✅ SUCCESS: Beat compiler optimization!\n");
        }
    } else {
        printf("Performance:      %.1f%% slower ⚠️\n", -improvement);
        printf("💡 Compiler optimization still superior\n");
    }
    
    printf("\nOptimization techniques used:\n");
    printf("- Zero register saves (eliminated 16+ stp/ldp instructions)\n");
    printf("- Complete loop unrolling (eliminated all branch overhead)\n");
    printf("- Pre-computed constants (eliminated repeated calculations)\n");
    printf("- Optimized instruction scheduling (better pipeline utilization)\n");
    
    printf("\nGlobal checksum: %u\n", global_checksum);
    
    free(ctx);
    free(src);
    free(reference_output);
    free(extreme_output);
    
    return 0;
}