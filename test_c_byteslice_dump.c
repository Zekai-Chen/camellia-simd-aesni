/*
 * Capture actual mem_ab and mem_cd from C implementation after byteslice
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Copy the relevant C code structures and macros
typedef uint8x16_t __m128i;

#define vmovdqa128(a, o) (o = a)
#define vmovdqa128_memld(a, o) (o = (*(const __m128i *)(a)))
#define vpshufb128(m, a, o) (o = (__m128i)vqtbl1q_u8((uint8x16_t)a, (uint8x16_t)m))

// transpose_4x4 macro
#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
    t2 = (__m128i)vzip2q_u32((uint32x4_t)x0, (uint32x4_t)x1); \
    x0 = (__m128i)vzip1q_u32((uint32x4_t)x0, (uint32x4_t)x1); \
    t1 = (__m128i)vzip1q_u32((uint32x4_t)x2, (uint32x4_t)x3); \
    x2 = (__m128i)vzip2q_u32((uint32x4_t)x2, (uint32x4_t)x3); \
    x3 = (__m128i)vzip2q_u64((uint64x2_t)t2, (uint64x2_t)x2); \
    x1 = (__m128i)vzip1q_u64((uint64x2_t)t2, (uint64x2_t)x2); \
    x2 = (__m128i)vzip2q_u64((uint64x2_t)x0, (uint64x2_t)t1); \
    x0 = (__m128i)vzip1q_u64((uint64x2_t)x0, (uint64x2_t)t1)

// Shuffle mask
static const uint8_t shufb_16x16b_bytes[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
static __m128i shufb_16x16b;

// byteslice_16x16b_fast implementation
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
    vmovdqa128(shufb_16x16b, a0); \
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
    vmovdqa128(st1, b1)

// Test plaintext
static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_mem(const char *label, __m128i *mem, int count) {
    printf("%s:\n", label);
    uint8_t buf[16];
    for (int i = 0; i < count; i++) {
        vst1q_u8(buf, (uint8x16_t)mem[i]);
        printf("  [%2d]: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x ", buf[j]);
        }
        printf("\n");
    }
}

int main(void) {
    __m128i x0, x1, x2, x3, x4, x5, x6, x7;
    __m128i y0, y1, y2, y3, y4, y5, y6, y7;
    __m128i ab[8], cd[8];
    __m128i tmp0, tmp1;

    // Initialize shuffle mask
    shufb_16x16b = vld1q_u8(shufb_16x16b_bytes);

    printf("========================================\n");
    printf("C Byteslice Actual Output\n");
    printf("========================================\n\n");

    // Load 16 identical blocks (reverse order like C code does)
    y7 = vld1q_u8(test_plaintext);
    y6 = vld1q_u8(test_plaintext);
    y5 = vld1q_u8(test_plaintext);
    y4 = vld1q_u8(test_plaintext);
    y3 = vld1q_u8(test_plaintext);
    y2 = vld1q_u8(test_plaintext);
    y1 = vld1q_u8(test_plaintext);
    y0 = vld1q_u8(test_plaintext);
    x7 = vld1q_u8(test_plaintext);
    x6 = vld1q_u8(test_plaintext);
    x5 = vld1q_u8(test_plaintext);
    x4 = vld1q_u8(test_plaintext);
    x3 = vld1q_u8(test_plaintext);
    x2 = vld1q_u8(test_plaintext);
    x1 = vld1q_u8(test_plaintext);
    x0 = vld1q_u8(test_plaintext);

    printf("Input (all 16 blocks identical):\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_plaintext[i]);
    }
    printf("\n\n");

    // Apply byteslice
    byteslice_16x16b_fast(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3,
                          y4, y5, y6, y7, ab[0], cd[0]);

    // Store results
    vmovdqa128(x0, ab[0]);
    vmovdqa128(x1, ab[1]);
    vmovdqa128(x2, ab[2]);
    vmovdqa128(x3, ab[3]);
    vmovdqa128(x4, ab[4]);
    vmovdqa128(x5, ab[5]);
    vmovdqa128(x6, ab[6]);
    vmovdqa128(x7, ab[7]);

    vmovdqa128(y0, cd[0]);
    vmovdqa128(y1, cd[1]);
    vmovdqa128(y2, cd[2]);
    vmovdqa128(y3, cd[3]);
    vmovdqa128(y4, cd[4]);
    vmovdqa128(y5, cd[5]);
    vmovdqa128(y6, cd[6]);
    vmovdqa128(y7, cd[7]);

    // Print results
    print_mem("mem_ab (left 64 bits)", ab, 8);
    printf("\n");
    print_mem("mem_cd (right 64 bits)", cd, 8);

    printf("\n========================================\n");
    printf("Analysis:\n");
    printf("========================================\n");
    printf("This is the ACTUAL format that C code produces.\n");
    printf("Assembly implementation must match this exactly.\n");

    return 0;
}
