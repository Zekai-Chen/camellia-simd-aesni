/*
 * C wrapper for byteslice - temporary solution until Assembly version is fixed
 */
#include <stdint.h>
#include <arm_neon.h>

// Type definitions to match C implementation
typedef uint8x16_t __m128i;

#define vmovdqa128(a, o) (o = a)

// Transpose macro (correct version from camellia_simd128_with_aes_instruction_set.c)
#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
do { \
    uint32x4_t _x0 = vreinterpretq_u32_u8(x0); \
    uint32x4_t _x1 = vreinterpretq_u32_u8(x1); \
    uint32x4_t _x2 = vreinterpretq_u32_u8(x2); \
    uint32x4_t _x3 = vreinterpretq_u32_u8(x3); \
    uint32x4_t _t2, _t1; \
    uint64x2_t __x0, __x1, __x2, __x3, __t1, __t2; \
    /* vpunpckhdq128(x1, x0, t2) = t2 = vzip2q_u32(x0, x1) */ \
    _t2 = vzip2q_u32(_x0, _x1); \
    /* vpunpckldq128(x1, x0, x0) = x0 = vzip1q_u32(x0, x1) */ \
    _x0 = vzip1q_u32(_x0, _x1); \
    /* vpunpckldq128(x3, x2, t1) = t1 = vzip1q_u32(x2, x3) */ \
    _t1 = vzip1q_u32(_x2, _x3); \
    /* vpunpckhdq128(x3, x2, x2) = x2 = vzip2q_u32(x2, x3) */ \
    _x2 = vzip2q_u32(_x2, _x3); \
    /* vpunpckhqdq128(t1, x0, x1) = x1 = vzip2q_u64(x0, t1) */ \
    __x0 = vreinterpretq_u64_u32(_x0); \
    __t1 = vreinterpretq_u64_u32(_t1); \
    __x1 = vzip2q_u64(__x0, __t1); \
    /* vpunpcklqdq128(t1, x0, x0) = x0 = vzip1q_u64(x0, t1) */ \
    __x0 = vzip1q_u64(__x0, __t1); \
    /* vpunpckhqdq128(x2, t2, x3) = x3 = vzip2q_u64(t2, x2) */ \
    __t2 = vreinterpretq_u64_u32(_t2); \
    __x2 = vreinterpretq_u64_u32(_x2); \
    __x3 = vzip2q_u64(__t2, __x2); \
    /* vpunpcklqdq128(x2, t2, x2) = x2 = vzip1q_u64(t2, x2) */ \
    __x2 = vzip1q_u64(__t2, __x2); \
    x0 = vreinterpretq_u8_u64(__x0); \
    x1 = vreinterpretq_u8_u64(__x1); \
    x2 = vreinterpretq_u8_u64(__x2); \
    x3 = vreinterpretq_u8_u64(__x3); \
} while(0)

#define vpshufb128(a, b, o) (o = vqtbl1q_u8(b, a))

// Shuffle pattern (must match .Lshufb_16x16b in Assembly)
static const uint8_t shufb_pattern_data[16] = {
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
};

// byteslice_16x16b_fast implementation (from camellia_simd128_with_aes_instruction_set.c)
#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, \
                              a3, b3, c3, d3, st0, st1) \
    vmovdqa128(d2, st0); \
    vmovdqa128(d3, st1); \
    transpose_4x4(a0, a1, a2, a3, d2, d3); \
    transpose_4x4(b0, b1, b2, b3, d2, d3); \
    vmovdqa128(st0, d2); \
    vmovdqa128(st1, d3); \
    vmovdqa128(a0, st0); \
    vmovdqa128(a1, st1); \
    transpose_4x4(c0, c1, c2, c3, a0, a1); \
    transpose_4x4(d0, d1, d2, d3, a0, a1); \
    __m128i shufb_16x16b_stack = vld1q_u8(shufb_pattern_data); \
    vmovdqa128(shufb_16x16b_stack, a0); \
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
    transpose_4x4(a0, b0, c0, d0, d2, d3); \
    transpose_4x4(a1, b1, c1, d1, d2, d3); \
    vmovdqa128(st0, d2); \
    vmovdqa128(st1, d3); \
    vmovdqa128(b0, st0); \
    vmovdqa128(b1, st1); \
    transpose_4x4(a2, b2, c2, d2, b0, b1); \
    transpose_4x4(a3, b3, c3, d3, b0, b1); \
    vmovdqa128(st0, b0); \
    vmovdqa128(st1, b1);

/*
 * byteslice_16x16b_wrapper - C wrapper for byteslice transformation
 *
 * Input: blocks[256] - 16 blocks in normal format (16 bytes each)
 * Output: blocks[256] - 16 blocks in byte-sliced format
 *         blocks[0-127] = AB state (8 vectors)
 *         blocks[128-255] = CD state (8 vectors)
 */
void byteslice_16x16b_wrapper(uint8_t *blocks)
{
    __m128i a0, b0, c0, d0, a1, b1, c1, d1;
    __m128i a2, b2, c2, d2, a3, b3, c3, d3;
    __m128i mem_ab[8], mem_cd[8];

    // Load 16 blocks
    a0 = vld1q_u8(&blocks[0 * 16]);   // block 0
    b0 = vld1q_u8(&blocks[1 * 16]);   // block 1
    c0 = vld1q_u8(&blocks[2 * 16]);   // block 2
    d0 = vld1q_u8(&blocks[3 * 16]);   // block 3
    a1 = vld1q_u8(&blocks[4 * 16]);   // block 4
    b1 = vld1q_u8(&blocks[5 * 16]);   // block 5
    c1 = vld1q_u8(&blocks[6 * 16]);   // block 6
    d1 = vld1q_u8(&blocks[7 * 16]);   // block 7
    a2 = vld1q_u8(&blocks[8 * 16]);   // block 8
    b2 = vld1q_u8(&blocks[9 * 16]);   // block 9
    c2 = vld1q_u8(&blocks[10 * 16]);  // block 10
    d2 = vld1q_u8(&blocks[11 * 16]);  // block 11
    a3 = vld1q_u8(&blocks[12 * 16]);  // block 12
    b3 = vld1q_u8(&blocks[13 * 16]);  // block 13
    c3 = vld1q_u8(&blocks[14 * 16]);  // block 14
    d3 = vld1q_u8(&blocks[15 * 16]);  // block 15

    // Apply byteslice (st0 = mem_ab[0], st1 = mem_cd[0])
    byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1,
                         a2, b2, c2, d2, a3, b3, c3, d3,
                         mem_ab[0], mem_cd[0]);

    // Store result: a0-d0, a1-d1, a2-d2, a3-d3 in parameter order
    vst1q_u8(&blocks[0 * 16], a0);
    vst1q_u8(&blocks[1 * 16], b0);
    vst1q_u8(&blocks[2 * 16], c0);
    vst1q_u8(&blocks[3 * 16], d0);
    vst1q_u8(&blocks[4 * 16], a1);
    vst1q_u8(&blocks[5 * 16], b1);
    vst1q_u8(&blocks[6 * 16], c1);
    vst1q_u8(&blocks[7 * 16], d1);
    vst1q_u8(&blocks[8 * 16], a2);
    vst1q_u8(&blocks[9 * 16], b2);
    vst1q_u8(&blocks[10 * 16], c2);
    vst1q_u8(&blocks[11 * 16], d2);
    vst1q_u8(&blocks[12 * 16], a3);
    vst1q_u8(&blocks[13 * 16], b3);
    vst1q_u8(&blocks[14 * 16], c3);
    vst1q_u8(&blocks[15 * 16], d3);
}
