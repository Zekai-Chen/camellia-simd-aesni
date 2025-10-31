/*
 * Compare C wrapper vs C reference byteslice
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

extern void byteslice_16x16b_wrapper(uint8_t *blocks);

// From C reference implementation
typedef uint8x16_t __m128i;
#define vmovdqa128(a, o) (o = a)

#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
do { \
    uint32x4_t _r0 = vreinterpretq_u32_u8(x0); \
    uint32x4_t _r1 = vreinterpretq_u32_u8(x1); \
    uint32x4_t _r2 = vreinterpretq_u32_u8(x2); \
    uint32x4_t _r3 = vreinterpretq_u32_u8(x3); \
    uint32x4_t _t2 = vzip2q_u32(_r0, _r1); \
    _r0 = vzip1q_u32(_r0, _r1); \
    uint32x4_t _t1 = vzip1q_u32(_r2, _r3); \
    _r2 = vzip2q_u32(_r2, _r3); \
    uint64x2_t _u1 = vreinterpretq_u64_u32(vzip1q_u32(_r2, _r0)); \
    x0 = vreinterpretq_u8_u64(vzip1q_u64(_u1, vreinterpretq_u64_u32(_r0))); \
    x1 = vreinterpretq_u8_u64(vzip2q_u64(_u1, vreinterpretq_u64_u32(_r0))); \
    uint64x2_t _u3 = vreinterpretq_u64_u32(vzip2q_u32(_r2, _t2)); \
    x2 = vreinterpretq_u8_u64(vzip1q_u64(_u3, vreinterpretq_u64_u32(_t2))); \
    x3 = vreinterpretq_u8_u64(vzip2q_u64(_u3, vreinterpretq_u64_u32(_t2))); \
} while(0)

#define vpshufb128(a, b, o) (o = vqtbl1q_u8(b, a))

static const uint8_t shufb_pattern_data[16] = {
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
};

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

void byteslice_reference(uint8_t *blocks) {
    __m128i a0, b0, c0, d0, a1, b1, c1, d1;
    __m128i a2, b2, c2, d2, a3, b3, c3, d3;
    __m128i mem_ab[8], mem_cd[8];

    // Load 16 blocks
    a0 = vld1q_u8(&blocks[0 * 16]);
    b0 = vld1q_u8(&blocks[1 * 16]);
    c0 = vld1q_u8(&blocks[2 * 16]);
    d0 = vld1q_u8(&blocks[3 * 16]);
    a1 = vld1q_u8(&blocks[4 * 16]);
    b1 = vld1q_u8(&blocks[5 * 16]);
    c1 = vld1q_u8(&blocks[6 * 16]);
    d1 = vld1q_u8(&blocks[7 * 16]);
    a2 = vld1q_u8(&blocks[8 * 16]);
    b2 = vld1q_u8(&blocks[9 * 16]);
    c2 = vld1q_u8(&blocks[10 * 16]);
    d2 = vld1q_u8(&blocks[11 * 16]);
    a3 = vld1q_u8(&blocks[12 * 16]);
    b3 = vld1q_u8(&blocks[13 * 16]);
    c3 = vld1q_u8(&blocks[14 * 16]);
    d3 = vld1q_u8(&blocks[15 * 16]);

    // Apply byteslice
    byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1,
                         a2, b2, c2, d2, a3, b3, c3, d3,
                         mem_ab[0], mem_cd[0]);

    // Store result
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

int main() {
    uint8_t blocks_wrapper[256];
    uint8_t blocks_reference[256];

    // Initialize with simple pattern: block i has all bytes = i
    for (int i = 0; i < 16; i++) {
        memset(&blocks_wrapper[i * 16], i, 16);
        memset(&blocks_reference[i * 16], i, 16);
    }

    // Call both implementations
    byteslice_16x16b_wrapper(blocks_wrapper);
    byteslice_reference(blocks_reference);

    // Compare
    printf("Comparing wrapper vs reference:\n");
    int match = 1;
    for (int i = 0; i < 256; i++) {
        if (blocks_wrapper[i] != blocks_reference[i]) {
            if (match) {
                printf("MISMATCH at byte %d: wrapper=%02x, reference=%02x\n",
                       i, blocks_wrapper[i], blocks_reference[i]);
            }
            match = 0;
        }
    }

    if (match) {
        printf("SUCCESS: Wrapper matches reference!\n");
    } else {
        printf("\nFAILURE: Wrapper does not match reference\n");
        printf("\nWrapper output:\n");
        for (int i = 0; i < 16; i++) {
            printf("  Vector %2d: ", i);
            for (int j = 0; j < 16; j++) {
                printf("%02x", blocks_wrapper[i * 16 + j]);
            }
            printf("\n");
        }
        printf("\nReference output:\n");
        for (int i = 0; i < 16; i++) {
            printf("  Vector %2d: ", i);
            for (int j = 0; j < 16; j++) {
                printf("%02x", blocks_reference[i * 16 + j]);
            }
            printf("\n");
        }
    }

    return match ? 0 : 1;
}
