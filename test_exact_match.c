/*
 * Test the exact match C Intrinsics implementation
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Include the exact match implementation */
#include "camellia_intrinsics_exact_match.c"

/* External Assembly function for comparison */
extern void camellia_encrypt_32blks_aarch64_neon_crypto(
    camellia_context *ctx, uint8_t *dst, const uint8_t *src);

void compare_byte_by_byte(const uint8_t* data1, const uint8_t* data2, int len, const char* label) {
    int matches = 0;
    int first_diff = -1;
    
    for(int i = 0; i < len; i++) {
        if(data1[i] == data2[i]) {
            matches++;
        } else if(first_diff == -1) {
            first_diff = i;
        }
    }
    
    printf("📊 %s: %d/%d bytes match (%.2f%%)\n", 
           label, matches, len, (100.0 * matches) / len);
    
    if(first_diff >= 0) {
        printf("  First difference at byte %d: Assembly=0x%02x, Intrinsics=0x%02x\n", 
               first_diff, data1[first_diff], data2[first_diff]);
    }
}

void debug_algorithm_step_by_step() {
    printf("\n🔍 Algorithm Step-by-Step Debug\n");
    printf("===============================\n");
    
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 16);  // Just one block for debugging
    uint8_t *dst = aligned_alloc(16, 16);
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    
    // Simple test data
    for (int i = 0; i < 16; i++) {
        src[i] = (uint8_t)(i + 13);
    }
    
    printf("Input block: ");
    for(int i = 0; i < 16; i++) {
        printf("%02x", src[i]);
        if((i + 1) % 8 == 0) printf(" ");
    }
    printf("\n");
    
    printf("Key table (first 16 bytes): ");
    for(int i = 0; i < 16; i++) {
        printf("%02x", ctx->key_table[i]);
        if((i + 1) % 8 == 0) printf(" ");
    }
    printf("\n");
    
    // Manual implementation of first few steps to debug
    uint8x16_t v0 = vld1q_u8(src);
    uint32_t block_counter = 32;  // w24
    uint32_t round_counter = 8;   // w25
    
    printf("\nFirst round transformation:\n");
    printf("Block counter: %d, Round counter: %d\n", block_counter, round_counter);
    
    // Key offset
    uint32_t key_offset = round_counter & 0xFF;
    printf("Key offset: %d, Key byte: 0x%02x\n", key_offset, ctx->key_table[key_offset]);
    
    free(ctx);
    free(src);
    free(dst);
}

int main() {
    printf("🎯 Exact Assembly Match Test\n");
    printf("============================\n");
    
    /* Allocate aligned memory */
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 512);
    uint8_t *dst_assembly = aligned_alloc(16, 512);
    uint8_t *dst_exact_match = aligned_alloc(16, 512);
    
    if (!ctx || !src || !dst_assembly || !dst_exact_match) {
        printf("❌ Memory allocation failed\n");
        return 1;
    }
    
    printf("📍 Memory alignment verified:\n");
    printf("Context: %p, Source: %p\n", ctx, src);
    
    /* Initialize with EXACT same data as successful tests */
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    
    for (int i = 0; i < 512; i++) {
        src[i] = (uint8_t)(i + 13);
    }
    
    printf("\n📊 Algorithm Flow Analysis:\n");
    printf("Assembly uses:\n");
    printf("- Block counter: starts at 32, decrements to 1\n");
    printf("- Round counter: starts at 8, decrements to 1\n");  
    printf("- Key offset: round_counter & 0xFF\n");
    printf("- Data dependency: XOR with block_counter as uint64_t\n");
    
    /* Clear outputs */
    memset(dst_assembly, 0xAA, 512);
    memset(dst_exact_match, 0xBB, 512);
    
    /* Test Assembly (reference) */
    printf("\n🚀 Running Assembly implementation...\n");
    camellia_encrypt_32blks_aarch64_neon_crypto(ctx, dst_assembly, src);
    printf("Assembly completed\n");
    
    /* Test Exact Match Intrinsics */
    printf("🚀 Running Exact Match Intrinsics implementation...\n");
    int result = camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(ctx, dst_exact_match, src);
    printf("Exact Match Intrinsics completed with return code: %d\n", result);
    
    /* Compare results */
    printf("\n📊 Detailed Comparison:\n");
    printf("=======================\n");
    
    printf("Assembly output (first 32 bytes):         ");
    for(int i = 0; i < 32; i++) {
        printf("%02x", dst_assembly[i]);
        if((i + 1) % 8 == 0 && i < 31) printf(" ");
    }
    printf("\n");
    
    printf("Exact Match Intrinsics (first 32 bytes):  ");
    for(int i = 0; i < 32; i++) {
        if(dst_assembly[i] == dst_exact_match[i]) {
            printf("\033[32m%02x\033[0m", dst_exact_match[i]);  // Green for match
        } else {
            printf("\033[31m%02x\033[0m", dst_exact_match[i]);  // Red for mismatch
        }
        if((i + 1) % 8 == 0 && i < 31) printf(" ");
    }
    printf("\n");
    
    compare_byte_by_byte(dst_assembly, dst_exact_match, 512, "Overall comparison");
    
    /* Check specific sections */
    compare_byte_by_byte(dst_assembly, dst_exact_match, 64, "First 64 bytes");
    compare_byte_by_byte(dst_assembly + 256, dst_exact_match + 256, 64, "Middle section");
    compare_byte_by_byte(dst_assembly + 448, dst_exact_match + 448, 64, "Last 64 bytes");
    
    /* Final assessment */
    printf("\n🎯 Final Assessment:\n");
    printf("====================\n");
    
    int perfect_match = memcmp(dst_assembly, dst_exact_match, 512) == 0;
    
    if(perfect_match) {
        printf("🎉 PERFECT MATCH ACHIEVED!\n");
        printf("✅ Exact Match Intrinsics produces identical output to Assembly\n");
        printf("✅ Algorithm implementation is now completely correct\n");
        printf("✅ Problem fully solved!\n");
    } else {
        printf("⚠️  Still has differences - need further algorithm analysis\n");
        
        // Show algorithm state for debugging
        debug_algorithm_step_by_step();
    }
    
    /* Cleanup */
    free(ctx);
    free(src);
    free(dst_assembly);
    free(dst_exact_match);
    
    return perfect_match ? 0 : 1;
}