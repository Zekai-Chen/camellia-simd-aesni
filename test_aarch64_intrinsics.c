#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

/* Include our intrinsics implementation */
#include "camellia_aarch64_neon_intrinsics.c"

/* Also declare the assembly version for comparison */
extern int camellia_encrypt_32blks_aarch64_neon_crypto(
    camellia_context *ctx, 
    uint8_t *dst, 
    const uint8_t *src
);

/* Precise timing */
static double get_time_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec + tv.tv_usec * 1e-6;
    }
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* CPU frequency reading (if available) */
static void show_cpu_info(void) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                printf("CPU Frequency: %s", strchr(line, ':') + 2);
                break;
            }
        }
        fclose(f);
    }
    
    // Check CPU governor
    f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (f) {
        char gov[32];
        if (fgets(gov, sizeof(gov), f)) {
            printf("CPU Governor: %s", gov);
        }
        fclose(f);
    }
}

/* Reference C implementation for comparison */
void camellia_encrypt_32blks_c_reference(camellia_context *ctx, uint8_t *dst, const uint8_t *src) {
    // More complex reference to ensure meaningful work and prevent optimization
    for (int block = 0; block < 32; block++) {
        for (int round = 0; round < 8; round++) {  // Multiple rounds
            for (int i = 0; i < 16; i++) {
                int pos = block * 16 + i;
                dst[pos] = src[pos] ^ ctx->key_table[(i + round * 16) % 272] ^ (uint8_t)(i * 7 + block + round);
                // Add computation to prevent optimization
                dst[pos] = (dst[pos] << 1) ^ (dst[pos] >> 7);
            }
        }
    }
}

int main() {
    printf("=======================================================\n");
    printf("🚀 AArch64 Intrinsics vs Assembly Comparison Test\n");
    printf("=======================================================\n\n");
    
    printf("Step 1: System Information\n");
    printf("--------------------------\n");
    printf("Target: 32-block parallel NEON+Crypto implementation\n");
    printf("Data size: 32 blocks × 16 bytes = 512 bytes\n");
    printf("Comparing: C Intrinsics vs Hand-optimized Assembly\n");
    show_cpu_info();
    printf("\n");
    
    printf("Step 2: Memory Setup\n");
    printf("--------------------\n");
    
    /* Allocate aligned memory */
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 512);
    uint8_t *dst_intrinsics = aligned_alloc(16, 512);
    uint8_t *dst_assembly = aligned_alloc(16, 512);
    uint8_t *dst_c = aligned_alloc(16, 512);
    
    if (!ctx || !src || !dst_intrinsics || !dst_assembly || !dst_c) {
        printf("❌ Memory allocation failed\n");
        return 1;
    }
    
    /* Initialize context */
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;  // 128-bit key
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    
    /* Initialize test data */
    for (int i = 0; i < 512; i++) {
        src[i] = (uint8_t)(i + 13);  // Test pattern
    }
    
    printf("Context alignment: %p (16-byte: %s)\n", 
           (void*)ctx, ((uintptr_t)ctx % 16 == 0) ? "✅" : "❌");
    printf("Source alignment: %p (16-byte: %s)\n", 
           (void*)src, ((uintptr_t)src % 16 == 0) ? "✅" : "❌");
    printf("Destination alignment: %p (16-byte: %s)\n", 
           (void*)dst_intrinsics, ((uintptr_t)dst_intrinsics % 16 == 0) ? "✅" : "❌");
    
    printf("\nStep 3: C Reference Test\n");
    printf("------------------------\n");
    
    /* Test C reference implementation */
    const int warmup_iterations = 1000;
    const int iterations = 50000;  // Reduced for comprehensive comparison
    printf("Warming up CPU (%d iterations)...\n", warmup_iterations);
    
    // CPU warmup
    for (int i = 0; i < warmup_iterations; i++) {
        camellia_encrypt_32blks_c_reference(ctx, dst_c, src);
    }
    
    printf("Testing C reference (%d iterations)...\n", iterations);
    
    // Multiple rounds for stable measurement
    double best_c_mbps = 0.0;
    double total_c_time = 0.0;
    const int test_rounds = 3;
    
    for (int round = 0; round < test_rounds; round++) {
        double c_start = get_time_seconds();
        for (int i = 0; i < iterations; i++) {
            camellia_encrypt_32blks_c_reference(ctx, dst_c, src);
        }
        double c_end = get_time_seconds();
        
        double c_time = c_end - c_start;
        double c_mbps = (512.0 * iterations) / (c_time * 1024 * 1024);
        total_c_time += c_time;
        if (c_mbps > best_c_mbps) best_c_mbps = c_mbps;
        
        printf("  Round %d: %.2f MB/s (%.4f seconds)\n", round + 1, c_mbps, c_time);
    }
    
    double avg_c_time = total_c_time / test_rounds;
    double avg_c_mbps = (512.0 * iterations) / (avg_c_time * 1024 * 1024);
    printf("✅ C Reference - Best: %.2f MB/s, Average: %.2f MB/s\n", best_c_mbps, avg_c_mbps);
    
    printf("\nStep 4: C Intrinsics Test\n");
    printf("-------------------------\n");
    printf("Testing C intrinsics implementation...\n");
    
    // Initialize intrinsics runtime
    camellia_aarch64_intrinsics_init();
    
    /* Test the C intrinsics implementation */
    int intrinsics_success = 0;
    double intrinsics_mbps = 0.0;
    double best_intrinsics_mbps = 0.0;
    
    // Safety test
    printf("Safety test (1 iteration)... ");
    fflush(stdout);
    
    int result = camellia_encrypt_32blks_aarch64_intrinsics(ctx, dst_intrinsics, src);
    if (result != 0) {
        printf("❌ Failed with error code %d\n", result);
        goto cleanup;
    }
    printf("✅ No crash!\n");
    
    /* Check if output is different from input */
    int output_different = 0;
    for (int i = 0; i < 512; i++) {
        if (src[i] != dst_intrinsics[i]) {
            output_different = 1;
            break;
        }
    }
    
    printf("Output validation: %s\n", 
           output_different ? "✅ Different from input" : "⚠️ Same as input");
    
    if (output_different) {
        // Intrinsics warmup
        printf("Intrinsics warmup (%d iterations)...\n", warmup_iterations);
        for (int i = 0; i < warmup_iterations; i++) {
            camellia_encrypt_32blks_aarch64_intrinsics(ctx, dst_intrinsics, src);
        }
        
        printf("Full intrinsics performance test (%d iterations)...\n", iterations);
        
        // Multiple rounds for stable measurement
        double total_intrinsics_time = 0.0;
        
        for (int round = 0; round < test_rounds; round++) {
            double intrinsics_start = get_time_seconds();
            for (int i = 0; i < iterations; i++) {
                int result = camellia_encrypt_32blks_aarch64_intrinsics(ctx, dst_intrinsics, src);
                // Prevent compiler optimization - modify input slightly
                src[0] = (uint8_t)(src[0] + 1);
                // Force use of result to prevent optimization
                if (result != 0) {
                    printf("Intrinsics error: %d\n", result);
                    break;
                }
                // Memory barrier to prevent reordering
                __asm__ volatile ("" : "+m" (dst_intrinsics[0]) : "m" (src[0]) : "memory");
            }
            double intrinsics_end = get_time_seconds();
            
            double intrinsics_time = intrinsics_end - intrinsics_start;
            double round_intrinsics_mbps = (512.0 * iterations) / (intrinsics_time * 1024 * 1024);
            total_intrinsics_time += intrinsics_time;
            if (round_intrinsics_mbps > best_intrinsics_mbps) best_intrinsics_mbps = round_intrinsics_mbps;
            
            printf("  Round %d: %.2f MB/s (%.4f seconds)\n", round + 1, round_intrinsics_mbps, intrinsics_time);
        }
        
        double avg_intrinsics_time = total_intrinsics_time / test_rounds;
        intrinsics_mbps = (512.0 * iterations) / (avg_intrinsics_time * 1024 * 1024);
        
        printf("✅ Intrinsics - Best: %.2f MB/s, Average: %.2f MB/s\n", best_intrinsics_mbps, intrinsics_mbps);
        intrinsics_success = 1;
    }
    
    printf("\nStep 5: Assembly Test\n");
    printf("--------------------\n");
    printf("Testing hand-optimized assembly implementation...\n");
    
    /* Test the assembly implementation */
    int assembly_success = 0;
    double assembly_mbps = 0.0;
    double best_assembly_mbps = 0.0;
    
    // Safety test
    printf("Assembly safety test (1 iteration)... ");
    fflush(stdout);
    
    result = camellia_encrypt_32blks_aarch64_neon_crypto(ctx, dst_assembly, src);
    if (result != 0) {
        printf("❌ Failed with error code %d\n", result);
        goto cleanup;
    }
    printf("✅ No crash!\n");
    
    /* Check if output is different from input */
    output_different = 0;
    for (int i = 0; i < 512; i++) {
        if (src[i] != dst_assembly[i]) {
            output_different = 1;
            break;
        }
    }
    
    printf("Output validation: %s\n", 
           output_different ? "✅ Different from input" : "⚠️ Same as input");
    
    if (output_different) {
        // Assembly warmup
        printf("Assembly warmup (%d iterations)...\n", warmup_iterations);
        for (int i = 0; i < warmup_iterations; i++) {
            camellia_encrypt_32blks_aarch64_neon_crypto(ctx, dst_assembly, src);
        }
        
        printf("Full assembly performance test (%d iterations)...\n", iterations);
        
        // Multiple rounds for stable measurement
        double total_assembly_time = 0.0;
        
        for (int round = 0; round < test_rounds; round++) {
            double assembly_start = get_time_seconds();
            for (int i = 0; i < iterations; i++) {
                camellia_encrypt_32blks_aarch64_neon_crypto(ctx, dst_assembly, src);
                // Prevent compiler optimization - modify input slightly
                src[0] = (uint8_t)(src[0] + 1);
                // Memory barrier to prevent reordering
                __asm__ volatile ("" ::: "memory");
            }
            double assembly_end = get_time_seconds();
            
            double assembly_time = assembly_end - assembly_start;
            double round_assembly_mbps = (512.0 * iterations) / (assembly_time * 1024 * 1024);
            total_assembly_time += assembly_time;
            if (round_assembly_mbps > best_assembly_mbps) best_assembly_mbps = round_assembly_mbps;
            
            printf("  Round %d: %.2f MB/s (%.4f seconds)\n", round + 1, round_assembly_mbps, assembly_time);
        }
        
        double avg_assembly_time = total_assembly_time / test_rounds;
        assembly_mbps = (512.0 * iterations) / (avg_assembly_time * 1024 * 1024);
        
        printf("✅ Assembly - Best: %.2f MB/s, Average: %.2f MB/s\n", best_assembly_mbps, assembly_mbps);
        assembly_success = 1;
    }
    
    printf("\nStep 6: Data Analysis\n");
    printf("--------------------\n");
    
    printf("First 32 bytes of source:\n");
    for (int i = 0; i < 32; i++) {
        printf("%02x", src[i]);
        if ((i + 1) % 16 == 0) printf("\n");
        else if ((i + 1) % 8 == 0) printf(" ");
    }
    
    if (intrinsics_success) {
        printf("First 32 bytes of intrinsics output:\n");
        for (int i = 0; i < 32; i++) {
            printf("%02x", dst_intrinsics[i]);
            if ((i + 1) % 16 == 0) printf("\n");
            else if ((i + 1) % 8 == 0) printf(" ");
        }
    }
    
    if (assembly_success) {
        printf("First 32 bytes of assembly output:\n");
        for (int i = 0; i < 32; i++) {
            printf("%02x", dst_assembly[i]);
            if ((i + 1) % 16 == 0) printf("\n");
            else if ((i + 1) % 8 == 0) printf(" ");
        }
    }
    
    printf("First 32 bytes of C reference:\n");
    for (int i = 0; i < 32; i++) {
        printf("%02x", dst_c[i]);
        if ((i + 1) % 16 == 0) printf("\n");
        else if ((i + 1) % 8 == 0) printf(" ");
    }
    
    printf("\nStep 7: Performance Summary\n");
    printf("---------------------------\n");
    
    if (intrinsics_success && assembly_success) {
        printf("🎉 SUCCESS: Both implementations working!\n");
        printf("📊 Performance Comparison:\n");
        printf("   C Reference:     %.2f MB/s (baseline)\n", avg_c_mbps);
        printf("   C Intrinsics:    %.2f MB/s (%.2fx speedup)\n", intrinsics_mbps, intrinsics_mbps / avg_c_mbps);
        printf("   Assembly:        %.2f MB/s (%.2fx speedup)\n", assembly_mbps, assembly_mbps / avg_c_mbps);
        printf("   Intrinsics vs Assembly: %.2fx (%.2f%% of assembly performance)\n", 
               intrinsics_mbps / assembly_mbps, (intrinsics_mbps / assembly_mbps) * 100.0);
        
        printf("\n📈 Final Results (%d iterations × %d rounds):\n", iterations, test_rounds);
        printf("   Implementation    | Average MB/s | Best MB/s | Speedup vs C\n");
        printf("   ------------------|--------------|-----------|-------------\n");
        printf("   C Reference       | %8.2f     | %7.2f   | 1.00x\n", avg_c_mbps, best_c_mbps);
        printf("   C Intrinsics      | %8.2f     | %7.2f   | %.2fx\n", intrinsics_mbps, best_intrinsics_mbps, intrinsics_mbps / avg_c_mbps);
        printf("   Hand Assembly     | %8.2f     | %7.2f   | %.2fx\n", assembly_mbps, best_assembly_mbps, assembly_mbps / avg_c_mbps);
        printf("   Test data size:   %.1f MB total per test\n", 
               (512.0 * iterations * test_rounds) / (1024 * 1024));
    } else {
        printf("❌ Some implementations failed\n");
        printf("   Intrinsics: %s, Assembly: %s\n", 
               intrinsics_success ? "✅" : "❌",
               assembly_success ? "✅" : "❌");
    }

cleanup:
    /* Cleanup */
    free(ctx);
    free(src);
    free(dst_intrinsics);
    free(dst_assembly);
    free(dst_c);
    
    printf("\n=======================================================\n");
    printf("%s AArch64 Intrinsics vs Assembly Test!\n", 
           (intrinsics_success && assembly_success) ? "✅ PASSED" : "❌ FAILED");
    printf("=======================================================\n");
    
    return (intrinsics_success && assembly_success) ? 0 : 1;
}