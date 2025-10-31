/*
 * Isolated byteslice comparison: C vs Assembly
 * This test compares ONLY the byteslice transformation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// C macro definitions (copied from camellia_simd128_with_aes_instruction_set.c)
#define vmovdqa128(a, o) (o = a)

#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
do { \
    uint32x4_t _r0 = vreinterpretq_u32_u8(x0); \
    uint32x4_t _r1 = vreinterpretq_u32_u8(x1); \
    uint32x4_t _r2 = vreinterpretq_u32_u8(x2); \
    uint32x4_t _r3 = vreinterpretq_u32_u8(x3); \
    uint32x4_t _t0, _t1, _t2, _t3; \
    _t0 = vzip1q_u32(_r1, _r0); \
    _r0 = vzip1q_u32(_r0, _r1); \
    _t1 = vzip1q_u32(_r3, _r2); \
    _r2 = vzip1q_u32(_r2, _r3); \
    uint64x2_t _u1 = vreinterpretq_u64_u32(vzip1q_u32(_r2, _r0)); \
    x0 = vreinterpretq_u8_u64(vzip1q_u64(_u1, vreinterpretq_u64_u32(_r0))); \
    x1 = vreinterpretq_u8_u64(vzip2q_u64(_u1, vreinterpretq_u64_u32(_r0))); \
    uint64x2_t _u3 = vreinterpretq_u64_u32(vzip2q_u32(_r2, _t0)); \
    x2 = vreinterpretq_u8_u64(vzip1q_u64(_u3, vreinterpretq_u64_u32(_t0))); \
    x3 = vreinterpretq_u8_u64(vzip2q_u64(_u3, vreinterpretq_u64_u32(_t0))); \
} while(0)

static uint8x16_t shufb_pattern;

#define vpshufb128(a, b, o) (o = vqtbl1q_u8(b, a))

// C byteslice_16x16b_fast implementation
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
    uint8x16_t shufb_16x16b_stack = a0; \
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

// Test input: simple pattern for easy debugging
static uint8_t test_input[256];

// Assembly byteslice wrapper
extern void test_byteslice_16x16b_asm(uint8_t *output, const uint8_t *input);

void init_test_input() {
    // Use simple incremental pattern
    for (int i = 0; i < 256; i++) {
        test_input[i] = i & 0xff;
    }
}

void byteslice_c_reference(uint8_t *output, const uint8_t *input) {
    uint8x16_t a0, b0, c0, d0, a1, b1, c1, d1;
    uint8x16_t a2, b2, c2, d2, a3, b3, c3, d3;
    uint8x16_t st0, st1;

    // Initialize shuffle pattern
    const uint8_t shuf_bytes[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    shufb_pattern = vld1q_u8(shuf_bytes);

    // Load input blocks
    a0 = vld1q_u8(&input[0 * 16]);    // block 0
    b0 = vld1q_u8(&input[1 * 16]);    // block 1
    c0 = vld1q_u8(&input[2 * 16]);    // block 2
    d0 = vld1q_u8(&input[3 * 16]);    // block 3
    a1 = vld1q_u8(&input[4 * 16]);    // block 4
    b1 = vld1q_u8(&input[5 * 16]);    // block 5
    c1 = vld1q_u8(&input[6 * 16]);    // block 6
    d1 = vld1q_u8(&input[7 * 16]);    // block 7
    a2 = vld1q_u8(&input[8 * 16]);    // block 8
    b2 = vld1q_u8(&input[9 * 16]);    // block 9
    c2 = vld1q_u8(&input[10 * 16]);   // block 10
    d2 = vld1q_u8(&input[11 * 16]);   // block 11
    a3 = vld1q_u8(&input[12 * 16]);   // block 12
    b3 = vld1q_u8(&input[13 * 16]);   // block 13
    c3 = vld1q_u8(&input[14 * 16]);   // block 14
    d3 = vld1q_u8(&input[15 * 16]);   // block 15

    // Apply byteslice
    byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1,
                         a2, b2, c2, d2, a3, b3, c3, d3,
                         st0, st1);

    // Store output: a0-a3, b0-b3 = AB (blocks 0-7), c0-c3, d0-d3 = CD (blocks 8-15)
    // But wait - we need to check parameter order!
    // After byteslice, output order should be:
    // a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, a3, b3, c3, d3

    vst1q_u8(&output[0 * 16], a0);
    vst1q_u8(&output[1 * 16], b0);
    vst1q_u8(&output[2 * 16], c0);
    vst1q_u8(&output[3 * 16], d0);
    vst1q_u8(&output[4 * 16], a1);
    vst1q_u8(&output[5 * 16], b1);
    vst1q_u8(&output[6 * 16], c1);
    vst1q_u8(&output[7 * 16], d1);
    vst1q_u8(&output[8 * 16], a2);
    vst1q_u8(&output[9 * 16], b2);
    vst1q_u8(&output[10 * 16], c2);
    vst1q_u8(&output[11 * 16], d2);
    vst1q_u8(&output[12 * 16], a3);
    vst1q_u8(&output[13 * 16], b3);
    vst1q_u8(&output[14 * 16], c3);
    vst1q_u8(&output[15 * 16], d3);
}

void print_vector(const char *label, int idx, const uint8_t *data) {
    printf("%s[%2d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[idx * 16 + i]);
    }
    printf("\n");
}

int main(void) {
    uint8_t output_c[256], output_asm[256];
    int mismatches = 0;

    init_test_input();

    printf("========================================\n");
    printf("Isolated Byteslice Comparison\n");
    printf("========================================\n\n");

    printf("Input: blocks 0-15 with pattern 00-ff\n");
    printf("  Block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", test_input[i]);
    printf("\n");
    printf("  Block 12: ");
    for (int i = 0; i < 16; i++) printf("%02x", test_input[12 * 16 + i]);
    printf("\n\n");

    // Run C byteslice
    byteslice_c_reference(output_c, test_input);

    // Run Assembly byteslice
    test_byteslice_16x16b_asm(output_asm, test_input);

    printf("Comparing output register-by-register:\n");
    printf("(Output order: a0-d0, a1-d1, a2-d2, a3-d3)\n");
    printf("(First 8 = AB, last 8 = CD)\n\n");

    for (int i = 0; i < 16; i++) {
        int match = 1;
        for (int j = 0; j < 16; j++) {
            if (output_c[i * 16 + j] != output_asm[i * 16 + j]) {
                match = 0;
                break;
            }
        }

        const char *label = i < 8 ? "AB" : "CD";
        int idx = i < 8 ? i : i - 8;

        if (!match) {
            printf("MISMATCH %s[%d]:\n", label, idx);
            print_vector("  C   ", i, output_c);
            print_vector("  ASM ", i, output_asm);
            printf("\n");
            mismatches++;
        } else {
            printf("OK %s[%d]: ", label, idx);
            for (int j = 0; j < 16; j++) printf("%02x", output_c[i * 16 + j]);
            printf("\n");
        }
    }

    printf("\n========================================\n");
    if (mismatches == 0) {
        printf("SUCCESS: All registers match!\n");
        return 0;
    } else {
        printf("FAILURE: %d register(s) don't match\n", mismatches);
        return 1;
    }
}
