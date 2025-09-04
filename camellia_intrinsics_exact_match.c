/*
 * Exact match C Intrinsics implementation - precisely follows Assembly
 */

#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

typedef struct {
    uint8_t key_table[272];
    uint32_t key_length;
} __attribute__((aligned(16))) camellia_context;

/*
 * Exact Assembly match - follows the exact same register usage and flow
 */
int camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(
    camellia_context *ctx, 
    uint8_t *dst, 
    const uint8_t *src
) {
    // Input validation (exact Assembly match)
    if (!ctx || !dst || !src) {
        return -1;
    }
    
    if (((uintptr_t)ctx & 0xF) != 0 || 
        ((uintptr_t)dst & 0xF) != 0 || 
        ((uintptr_t)src & 0xF) != 0) {
        return -1;
    }
    
    // Assembly register mapping:
    // x21 = ctx, x22 = src_ptr, x23 = dst_ptr, w24 = block_counter, w25 = round_counter
    uint8_t *src_ptr = (uint8_t *)src;      // x22
    uint8_t *dst_ptr = (uint8_t *)dst;      // x23
    uint32_t block_counter = 32;            // w24 - starts at 32, decrements
    
    // .Lprocess_loop:
    while (block_counter != 0) {
        // Load source block: ld1 {v0.16b}, [x22], #16
        uint8x16_t v0 = vld1q_u8(src_ptr);
        src_ptr += 16;
        
        // Round counter: mov w25, #8
        uint32_t round_counter = 8;         // w25 - starts at 8, decrements
        
        // .Lround_loop:
        while (round_counter != 0) {
            // Key offset calculation: add x26, x21, #0; and x27, x25, #0xFF; add x26, x26, x27
            uint32_t x27 = round_counter & 0xFF;  // and x27, x25, #0xFF
            uint8_t *key_ptr = ctx->key_table + x27;  // add x26, x26, x27
            
            // Load key: ld1 {v1.16b}, [x26]
            uint8x16_t v1 = vld1q_u8(key_ptr);
            
            // XOR with key: eor v0.16b, v0.16b, v1.16b
            v0 = veorq_u8(v0, v1);
            
            // Add block and round index: dup v2.16b, w24; dup v3.16b, w25
            uint8x16_t v2 = vdupq_n_u8((uint8_t)(block_counter & 0xFF));  // w24
            uint8x16_t v3 = vdupq_n_u8((uint8_t)(round_counter & 0xFF));  // w25
            
            // eor v0.16b, v0.16b, v2.16b; eor v0.16b, v0.16b, v3.16b
            v0 = veorq_u8(v0, v2);
            v0 = veorq_u8(v0, v3);
            
            // AES S-box: eor v4.16b, v4.16b, v4.16b; aese v0.16b, v4.16b
            uint8x16_t v4 = vdupq_n_u8(0);  // eor v4.16b, v4.16b, v4.16b
            v0 = vaeseq_u8(v0, v4);         // aese v0.16b, v4.16b
            
            // Undo MixColumns: aesimc v0.16b, v0.16b
            v0 = vaesimcq_u8(v0);
            
            // Bit rotation: shl v5.16b, v0.16b, #1; ushr v6.16b, v0.16b, #7; eor v0.16b, v5.16b, v6.16b
            uint8x16_t v5 = vshlq_n_u8(v0, 1);   // shl v5.16b, v0.16b, #1
            uint8x16_t v6 = vshrq_n_u8(v0, 7);   // ushr v6.16b, v0.16b, #7
            v0 = veorq_u8(v5, v6);               // eor v0.16b, v5.16b, v6.16b
            
            // Data dependency: mov v7.d[0], x24; eor v0.16b, v0.16b, v7.16b
            uint64_t x24_val = (uint64_t)block_counter;  // x24 value
            uint8x16_t v7 = vcombine_u8(vcreate_u8(x24_val), vcreate_u8(0));  // mov v7.d[0], x24
            v0 = veorq_u8(v0, v7);               // eor v0.16b, v0.16b, v7.16b
            
            // Decrement round counter: subs w25, w25, #1
            round_counter--;
        }
        
        // Store result: st1 {v0.16b}, [x23], #16
        vst1q_u8(dst_ptr, v0);
        dst_ptr += 16;
        
        // Decrement block counter: subs w24, w24, #1
        block_counter--;
    }
    
    return 0;  // mov w0, #0
}

/* Test function */
int test_exact_match() {
    printf("🎯 Testing Exact Assembly Match Implementation\n");
    printf("==============================================\n");
    
    camellia_context *ctx = aligned_alloc(16, sizeof(camellia_context));
    uint8_t *src = aligned_alloc(16, 512);
    uint8_t *dst = aligned_alloc(16, 512);
    
    if (!ctx || !src || !dst) {
        printf("❌ Memory allocation failed\n");
        return -1;
    }
    
    // Exact same initialization as successful tests
    memset(ctx, 0, sizeof(*ctx));
    ctx->key_length = 16;
    for (int i = 0; i < 272; i++) {
        ctx->key_table[i] = (uint8_t)(i ^ 0x5A);
    }
    
    for (int i = 0; i < 512; i++) {
        src[i] = (uint8_t)(i + 13);
    }
    
    memset(dst, 0xCC, 512);
    
    printf("📊 Running exact match implementation...\n");
    int result = camellia_encrypt_32blks_aarch64_neon_intrinsics_exact_match(ctx, dst, src);
    
    printf("Function returned: %d\n", result);
    printf("Output (first 32 bytes): ");
    for(int i = 0; i < 32; i++) {
        printf("%02x", dst[i]);
        if((i + 1) % 8 == 0 && i < 31) printf(" ");
    }
    printf("\n");
    
    printf("\nExpected Assembly: 2958d408f8e2c65b 5402ccb808e3ec17 68f95941900f1d51 2b45e4e34c4ddf3a\n");
    
    free(ctx);
    free(src);
    free(dst);
    
    return result;
}