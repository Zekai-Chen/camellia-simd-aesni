/*
 * Test assembly byteslice against the REAL Kivilinna byteslice_16x16b_fast
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Shuffle pattern from Kivilinna implementation
static const uint8_t shufb_16x16b[16] __attribute__((aligned(16))) = {
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
};

// Kivilinna macros
#define vmovdqa128(mem, reg) (reg) = (mem)
#define vpshufb128(pat, data, out) (out) = vqtbl1q_u8((data), (pat))

#define transpose_4x4(x0, x1, x2, x3, t0, t1) do { \
    t0 = vreinterpretq_s32_u8(vtrn1q_u32(vreinterpretq_u32_u8(x0), vreinterpretq_u32_u8(x1))); \
    t1 = vreinterpretq_s32_u8(vtrn2q_u32(vreinterpretq_u32_u8(x0), vreinterpretq_u32_u8(x1))); \
    x0 = vreinterpretq_u8_u64(vtrn1q_u64(vreinterpretq_u64_s32(t0), vreinterpretq_u64_u32(vreinterpretq_u32_u8(x2)))); \
    x1 = vreinterpretq_u8_u64(vtrn1q_u64(vreinterpretq_u64_s32(t1), vreinterpretq_u64_u32(vreinterpretq_u32_u8(x3)))); \
    x2 = vreinterpretq_u8_u64(vtrn2q_u64(vreinterpretq_u64_s32(t0), vreinterpretq_u64_u32(vreinterpretq_u32_u8(x2)))); \
    x3 = vreinterpretq_u8_u64(vtrn2q_u64(vreinterpretq_u64_s32(t1), vreinterpretq_u64_u32(vreinterpretq_u32_u8(x3)))); \
} while(0)

// Exact copy of Kivilinna's byteslice_16x16b_fast macro
#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, \
                              a3, b3, c3, d3, st0, st1) \
    vmovdqa128(d2, st0); \
    vmovdqa128(d3, st1); \
    transpose_4x4(a0, a1, a2, a3, d2, d3); \
    transpose_4x4(b0, b1, b2, b3, d2, d3); \
    vmovdqa128(st0, d2); \
    vmovdqa128(st1, d3); \
    \
    vmovdqa128(a0, st0); \
    vmovdqa128(a1, st1); \
    transpose_4x4(c0, c1, c2, c3, a0, a1); \
    transpose_4x4(d0, d1, d2, d3, a0, a1); \
    \
    vmovdqa128(shufb_pattern, a0); \
    vmovdqa128(st1, a1); \
    vpshufb128(a0, a2, a2); \
    vpshufb128(a0, a3, a3); \
    vpshufb128(a0, b0, b0); \
    vpshufb128(a0, b1, b1); \
    vpshufb128(a0, b2, b2); \
    vpshufb128(a0, b3, b3); \
    vpshufb128(a0, a1, a1); \
    vpshufb128(a0, c0, c0); \
    vpshufb128(a0, c1, c1); \
    vpshufb128(a0, c2, c2); \
    vpshufb128(a0, c3, c3); \
    vpshufb128(a0, d0, d0); \
    vpshufb128(a0, d1, d1); \
    vpshufb128(a0, d2, d2); \
    vpshufb128(a0, d3, d3); \
    vmovdqa128(d3, st1); \
    vmovdqa128(st0, d3); \
    vpshufb128(a0, d3, a0); \
    vmovdqa128(d2, st0); \
    \
    transpose_4x4(a0, b0, c0, d0, d2, d3); \
    transpose_4x4(a1, b1, c1, d1, d2, d3); \
    vmovdqa128(st0, d2); \
    vmovdqa128(st1, d3); \
    \
    vmovdqa128(b0, st0); \
    vmovdqa128(b1, st1); \
    transpose_4x4(a2, b2, c2, d2, b0, b1); \
    transpose_4x4(a3, b3, c3, d3, b0, b1); \
    vmovdqa128(st0, b0); \
    vmovdqa128(st1, b1);

// C wrapper for Kivilinna byteslice
void byteslice_16x16b_c(uint8_t *data) {
    uint8x16_t a0, b0, c0, d0, a1, b1, c1, d1;
    uint8x16_t a2, b2, c2, d2, a3, b3, c3, d3;
    uint8x16_t st0, st1, shufb_pattern;

    // Load data
    a0 = vld1q_u8(&data[0 * 16]);
    b0 = vld1q_u8(&data[1 * 16]);
    c0 = vld1q_u8(&data[2 * 16]);
    d0 = vld1q_u8(&data[3 * 16]);
    a1 = vld1q_u8(&data[4 * 16]);
    b1 = vld1q_u8(&data[5 * 16]);
    c1 = vld1q_u8(&data[6 * 16]);
    d1 = vld1q_u8(&data[7 * 16]);
    a2 = vld1q_u8(&data[8 * 16]);
    b2 = vld1q_u8(&data[9 * 16]);
    c2 = vld1q_u8(&data[10 * 16]);
    d2 = vld1q_u8(&data[11 * 16]);
    a3 = vld1q_u8(&data[12 * 16]);
    b3 = vld1q_u8(&data[13 * 16]);
    c3 = vld1q_u8(&data[14 * 16]);
    d3 = vld1q_u8(&data[15 * 16]);

    // Load shuffle pattern
    shufb_pattern = vld1q_u8(shufb_16x16b);

    // Apply byteslice
    byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1,
                         a2, b2, c2, d2, a3, b3, c3, d3,
                         st0, st1);

    // Store result
    vst1q_u8(&data[0 * 16], a0);
    vst1q_u8(&data[1 * 16], b0);
    vst1q_u8(&data[2 * 16], c0);
    vst1q_u8(&data[3 * 16], d0);
    vst1q_u8(&data[4 * 16], a1);
    vst1q_u8(&data[5 * 16], b1);
    vst1q_u8(&data[6 * 16], c1);
    vst1q_u8(&data[7 * 16], d1);
    vst1q_u8(&data[8 * 16], a2);
    vst1q_u8(&data[9 * 16], b2);
    vst1q_u8(&data[10 * 16], c2);
    vst1q_u8(&data[11 * 16], d2);
    vst1q_u8(&data[12 * 16], a3);
    vst1q_u8(&data[13 * 16], b3);
    vst1q_u8(&data[14 * 16], c3);
    vst1q_u8(&data[15 * 16], d3);
}

// Assembly byteslice (from our implementation)
extern void test_byteslice_with_kivilinna(uint8_t *output, const uint8_t *input);

int main(void) {
    uint8_t input[256];
    uint8_t output_c[256];
    uint8_t output_asm[256];

    printf("========================================\n");
    printf("Byteslice: C (Kivilinna) vs Assembly\n");
    printf("========================================\n\n");

    // Initialize input with distinctive pattern
    for (int i = 0; i < 256; i++) {
        input[i] = (i / 16) * 16 + (i % 16);
    }

    // Test C byteslice
    memcpy(output_c, input, 256);
    byteslice_16x16b_c(output_c);

    // Test Assembly byteslice
    test_byteslice_with_kivilinna(output_asm, input);

    // Compare results
    int errors = 0;
    printf("Comparing byteslice outputs...\n");
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            if (errors < 24) {
                printf("  Byte %3d: C=%02x  ASM=%02x  (diff=%02x)\n",
                       i, output_c[i], output_asm[i], output_c[i] ^ output_asm[i]);
            }
            errors++;
        }
    }

    printf("\nResult: %d/256 bytes match\n", 256 - errors);

    if (errors == 0) {
        printf("✓ BYTESLICE MATCH\n");
        return 0;
    } else {
        printf("✗ BYTESLICE MISMATCH\n");
        return 1;
    }
}
