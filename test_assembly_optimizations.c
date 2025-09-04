/*
 * Test Assembly optimization effectiveness
 * Compare multiple Assembly versions against C Intrinsics
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

/* Function prototypes */
extern int camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

extern int camellia_encrypt_32blks_aarch64_neon_crypto_optimized(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

extern int camellia_encrypt_32blks_aarch64_neon_crypto_ultra_optimized(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

extern int camellia_encrypt_32blks_aarch64_neon_crypto_block_unrolled(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

volatile uint32_t global_checksum = 0;

typedef struct {
    const char *name;
    int (*function)(camellia_context *ctx, uint8_t *dst, const uint8_t *src);
    double performance;
    int correct;
} implementation_t;

int main() {
    printf("🚀 Assembly Optimization Performance Test\n");
    printf("=========================================\n");
    printf("Target: Beat C Intrinsics performance (716 MB/s)\n\n");
    
    // Test implementations
    implementation_t implementations[] = {
        {"C Intrinsics (Reference)", camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match, 0.0, 1},
        {"Assembly Original", camellia_encrypt_32blks_aarch64_neon_crypto_optimized, 0.0, 0},
        {"Assembly Ultra-Optimized", camellia_encrypt_32blks_aarch64_neon_crypto_ultra_optimized, 0.0, 0},
        {"Assembly Block-Unrolled", camellia_encrypt_32blks_aarch64_neon_crypto_block_unrolled, 0.0, 0}
    };
    int num_implementations = sizeof(implementations) / sizeof(implementations[0]);
    
    // Setup test data
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 512);
    uint8_t *reference_output = aligned_alloc(16, 512);
    uint8_t *test_output = aligned_alloc(16, 512);
    
    if (!ctx || !src || !reference_output || !test_output) {
        printf("❌ Memory allocation failed\n");
        return 1;
    }
    
    // Initialize test data
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    for (int i = 0; i < 512; i++) {
        src[i] = (uint8_t)(i + 13);
    }
    
    printf("1. Correctness Testing:\n");
    printf("=======================\n");
    
    // Get reference output (C Intrinsics)
    implementations[0].function(ctx, reference_output, src);
    printf("✅ %s: Reference implementation\n", implementations[0].name);
    
    // Test each Assembly implementation
    for (int i = 1; i < num_implementations; i++) {
        memset(test_output, 0xCC, 512);
        int result = implementations[i].function(ctx, test_output, src);
        
        if (result != 0) {
            printf("❌ %s: Function returned error %d\n", implementations[i].name, result);
            implementations[i].correct = 0;
            continue;
        }
        
        int match = memcmp(reference_output, test_output, 512) == 0;
        implementations[i].correct = match;
        
        if (match) {
            printf("✅ %s: Output matches reference\n", implementations[i].name);
        } else {
            printf("❌ %s: Output differs from reference\n", implementations[i].name);
            
            // Show first difference
            for (int j = 0; j < 512; j++) {
                if (reference_output[j] != test_output[j]) {
                    printf("   First difference at byte %d: expected %02x, got %02x\n", 
                           j, reference_output[j], test_output[j]);
                    break;
                }
            }
        }
    }
    
    printf("\n2. Performance Testing:\n");
    printf("=======================\n");
    
    int iterations = 10000;
    printf("Running %d iterations per implementation...\n\n", iterations);
    
    for (int i = 0; i < num_implementations; i++) {
        if (i > 0 && !implementations[i].correct) {
            printf("⚠️  Skipping %s (incorrect output)\n", implementations[i].name);
            continue;
        }
        
        printf("🔧 Testing %s...\n", implementations[i].name);
        
        double start_time = get_time_seconds();
        
        for (int iter = 0; iter < iterations; iter++) {
            implementations[i].function(ctx, test_output, src);
            global_checksum += test_output[0] + test_output[256] + test_output[511];
        }
        
        double end_time = get_time_seconds();
        double total_time = end_time - start_time;
        
        // Calculate performance
        double total_mb = (double)(iterations * 512) / (1024.0 * 1024.0);
        double mbps = total_mb / total_time;
        implementations[i].performance = mbps;
        
        printf("   Time: %.6f seconds\n", total_time);
        printf("   Performance: %.2f MB/s\n", mbps);
        printf("   Per iteration: %.3f µs\n", (total_time * 1e6) / iterations);
        
        if (i == 0) {
            printf("   Status: Reference baseline\n");
        } else {
            double improvement = (mbps / implementations[0].performance - 1.0) * 100.0;
            if (improvement > 0) {
                printf("   Status: %.1f%% FASTER than reference ✅\n", improvement);
            } else {
                printf("   Status: %.1f%% slower than reference ⚠️\n", -improvement);
            }
        }
        printf("\n");
    }
    
    printf("3. Performance Summary:\n");
    printf("======================\n");
    printf("Implementation                    Performance    vs Reference\n");
    printf("--------------------------------------------------------\n");
    
    for (int i = 0; i < num_implementations; i++) {
        if (i > 0 && !implementations[i].correct) {
            printf("%-30s   INCORRECT     N/A\n", implementations[i].name);
            continue;
        }
        
        if (i == 0) {
            printf("%-30s   %7.1f MB/s   (baseline)\n", 
                   implementations[i].name, implementations[i].performance);
        } else {
            double ratio = implementations[i].performance / implementations[0].performance;
            printf("%-30s   %7.1f MB/s   %.3fx\n", 
                   implementations[i].name, implementations[i].performance, ratio);
        }
    }
    
    printf("\n4. Analysis:\n");
    printf("============\n");
    
    // Find best performing implementation
    double best_performance = 0.0;
    int best_impl = -1;
    for (int i = 0; i < num_implementations; i++) {
        if (implementations[i].correct && implementations[i].performance > best_performance) {
            best_performance = implementations[i].performance;
            best_impl = i;
        }
    }
    
    if (best_impl == 0) {
        printf("🎯 C Intrinsics remains the fastest implementation.\n");
        printf("   Modern compilers are extremely effective at optimization.\n");
        printf("   Consider focusing on algorithm-level improvements instead.\n");
    } else {
        printf("🎉 SUCCESS: %s achieved best performance!\n", implementations[best_impl].name);
        double improvement = (best_performance / implementations[0].performance - 1.0) * 100.0;
        printf("   %.1f%% improvement over C Intrinsics\n", improvement);
        
        if (best_performance > 800.0) {
            printf("🏆 OUTSTANDING: Achieved >800 MB/s performance!\n");
        } else if (best_performance > 716.0) {
            printf("✅ EXCELLENT: Beat the 716 MB/s target!\n");
        }
    }
    
    // Optimization recommendations
    printf("\n5. Optimization Insights:\n");
    printf("========================\n");
    
    int ultra_idx = 2; // Ultra-optimized index
    int block_idx = 3; // Block-unrolled index
    
    if (implementations[ultra_idx].correct) {
        printf("Ultra-optimization effects:\n");
        printf("- Loop unrolling: Eliminated inner loop overhead\n");
        printf("- Register optimization: Reduced register pressure\n");
        printf("- Instruction reordering: Improved pipeline utilization\n");
        printf("- Result: %.1f%% performance change\n", 
               (implementations[ultra_idx].performance / implementations[1].performance - 1.0) * 100.0);
    }
    
    if (implementations[block_idx].correct) {
        printf("\nBlock unrolling effects:\n");
        printf("- 2-way block processing: Increased instruction-level parallelism\n");
        printf("- Reduced loop overhead: Half the number of loop iterations\n");
        printf("- Better pipeline utilization: More work per iteration\n");
        printf("- Result: %.1f%% performance change\n", 
               (implementations[block_idx].performance / implementations[1].performance - 1.0) * 100.0);
    }
    
    printf("\nGlobal checksum (anti-optimization): %u\n", global_checksum);
    
    // Cleanup
    free(ctx);
    free(src);
    free(reference_output);
    free(test_output);
    
    return 0;
}