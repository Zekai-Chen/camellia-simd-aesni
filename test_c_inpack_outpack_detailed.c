/*
 * Test C implementation inpack+outunpack with detailed variable tracking
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// From the C implementation
#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
    t2 = vzip2q_s32(vreinterpretq_s32_u8(x0), vreinterpretq_s32_u8(x1)); \
    x0 = vreinterpretq_u8_s32(vzip1q_s32(vreinterpretq_s32_u8(x0), vreinterpretq_s32_u8(x1))); \
    t1 = vzip1q_s32(vreinterpretq_s32_u8(x2), vreinterpretq_s32_u8(x3)); \
    x2 = vreinterpretq_u8_s32(vzip2q_s32(vreinterpretq_s32_u8(x2), vreinterpretq_s32_u8(x3))); \
    x1 = vreinterpretq_u8_s64(vzip2q_s64(vreinterpretq_s64_u8(x0), vreinterpretq_s64_s32(t1))); \
    x0 = vreinterpretq_u8_s64(vzip1q_s64(vreinterpretq_s64_u8(x0), vreinterpretq_s64_s32(t1))); \
    x3 = vreinterpretq_u8_s64(vzip2q_s64(vreinterpretq_s64_s32(t2), vreinterpretq_s64_u8(x2))); \
    x2 = vreinterpretq_u8_s64(vzip1q_s64(vreinterpretq_s64_s32(t2), vreinterpretq_s64_u8(x2)));

static const uint8_t shufb_16x16b[16] = {
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
};

#define byteslice_16x16b(a0, b0, c0, d0, a1, b1, c1, d1, \
                         a2, b2, c2, d2, a3, b3, c3, d3) \
{ \
    uint8x16_t t0, t1, t2, t3, t4, t5; \
    uint8x16_t shuf = vld1q_u8(shufb_16x16b); \
    int32x4_t tt1, tt2; \
    \
    t4 = d2; \
    t5 = d3; \
    \
    transpose_4x4(a0, a1, a2, a3, tt1, tt2); \
    transpose_4x4(b0, b1, b2, b3, tt1, tt2); \
    \
    d2 = t4; \
    d3 = t5; \
    t4 = a0; \
    t5 = a1; \
    \
    transpose_4x4(c0, c1, c2, c3, tt1, tt2); \
    transpose_4x4(d0, d1, d2, d3, tt1, tt2); \
    \
    t3 = t4; \
    t0 = t5; \
    \
    a0 = vqtbl1q_u8(t3, shuf); \
    a1 = vqtbl1q_u8(t0, shuf); \
    a2 = vqtbl1q_u8(a2, shuf); \
    a3 = vqtbl1q_u8(a3, shuf); \
    b0 = vqtbl1q_u8(b0, shuf); \
    b1 = vqtbl1q_u8(b1, shuf); \
    b2 = vqtbl1q_u8(b2, shuf); \
    b3 = vqtbl1q_u8(b3, shuf); \
    c0 = vqtbl1q_u8(c0, shuf); \
    c1 = vqtbl1q_u8(c1, shuf); \
    c2 = vqtbl1q_u8(c2, shuf); \
    c3 = vqtbl1q_u8(c3, shuf); \
    d0 = vqtbl1q_u8(d0, shuf); \
    d1 = vqtbl1q_u8(d1, shuf); \
    d2 = vqtbl1q_u8(d2, shuf); \
    d3 = vqtbl1q_u8(d3, shuf); \
    \
    transpose_4x4(a0, b0, c0, d0, tt1, tt2); \
    \
    t4 = d2; \
    t5 = d3; \
    \
    transpose_4x4(a1, b1, c1, d1, tt1, tt2); \
    \
    t0 = b0; \
    t1 = b1; \
    transpose_4x4(a2, b2, c2, d2, tt1, tt2); \
    transpose_4x4(a3, b3, c3, d3, tt1, tt2); \
    \
    b0 = t0; \
    b1 = t1; \
    d2 = t4; \
    d3 = t5; \
}

int main(void) {
    uint8_t input[256];
    uint8_t output[256];

    printf("C inpack+outunpack detailed test\n");
    printf("=================================\n\n");

    // Fill input
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }

    uint64_t key = 0x0123456789abcdefULL;
    uint8x16_t key_vec = vreinterpretq_u8_u64(vdupq_n_u64(key));

    // Load 16 blocks in FORWARD order (like memory layout)
    uint8x16_t v0 = vld1q_u8(input + 0 * 16);
    uint8x16_t v1 = vld1q_u8(input + 1 * 16);
    uint8x16_t v2 = vld1q_u8(input + 2 * 16);
    uint8x16_t v3 = vld1q_u8(input + 3 * 16);
    uint8x16_t v4 = vld1q_u8(input + 4 * 16);
    uint8x16_t v5 = vld1q_u8(input + 5 * 16);
    uint8x16_t v6 = vld1q_u8(input + 6 * 16);
    uint8x16_t v7 = vld1q_u8(input + 7 * 16);
    uint8x16_t v8 = vld1q_u8(input + 8 * 16);
    uint8x16_t v9 = vld1q_u8(input + 9 * 16);
    uint8x16_t v10 = vld1q_u8(input + 10 * 16);
    uint8x16_t v11 = vld1q_u8(input + 11 * 16);
    uint8x16_t v12 = vld1q_u8(input + 12 * 16);
    uint8x16_t v13 = vld1q_u8(input + 13 * 16);
    uint8x16_t v14 = vld1q_u8(input + 14 * 16);
    uint8x16_t v15 = vld1q_u8(input + 15 * 16);

    // XOR with key (pre-whitening)
    v0 = veorq_u8(v0, key_vec);
    v1 = veorq_u8(v1, key_vec);
    v2 = veorq_u8(v2, key_vec);
    v3 = veorq_u8(v3, key_vec);
    v4 = veorq_u8(v4, key_vec);
    v5 = veorq_u8(v5, key_vec);
    v6 = veorq_u8(v6, key_vec);
    v7 = veorq_u8(v7, key_vec);
    v8 = veorq_u8(v8, key_vec);
    v9 = veorq_u8(v9, key_vec);
    v10 = veorq_u8(v10, key_vec);
    v11 = veorq_u8(v11, key_vec);
    v12 = veorq_u8(v12, key_vec);
    v13 = veorq_u8(v13, key_vec);
    v14 = veorq_u8(v14, key_vec);
    v15 = veorq_u8(v15, key_vec);

    // Rearrange to REVERSE order (like C inpack does)
    uint8x16_t x0 = v15;  // block 15
    uint8x16_t x1 = v14;  // block 14
    uint8x16_t x2 = v13;
    uint8x16_t x3 = v12;
    uint8x16_t x4 = v11;
    uint8x16_t x5 = v10;
    uint8x16_t x6 = v9;
    uint8x16_t x7 = v8;
    uint8x16_t x8 = v7;   // block 7
    uint8x16_t x9 = v6;
    uint8x16_t x10 = v5;
    uint8x16_t x11 = v4;
    uint8x16_t x12 = v3;
    uint8x16_t x13 = v2;
    uint8x16_t x14 = v1;
    uint8x16_t x15 = v0;  // block 0

    // Forward byteslice
    byteslice_16x16b(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15);

    // Now x0-x7 = AB, x8-x15 = CD
    // Reverse byteslice with interleaved order
    byteslice_16x16b(x8, x12, x0, x4, x9, x13, x1, x5, x10, x14, x2, x6, x11, x15, x3, x7);

    // Post-whitening XOR (in parameter order!)
    x8 = veorq_u8(x8, key_vec);
    x12 = veorq_u8(x12, key_vec);
    x0 = veorq_u8(x0, key_vec);
    x4 = veorq_u8(x4, key_vec);
    x9 = veorq_u8(x9, key_vec);
    x13 = veorq_u8(x13, key_vec);
    x1 = veorq_u8(x1, key_vec);
    x5 = veorq_u8(x5, key_vec);
    x10 = veorq_u8(x10, key_vec);
    x14 = veorq_u8(x14, key_vec);
    x2 = veorq_u8(x2, key_vec);
    x6 = veorq_u8(x6, key_vec);
    x11 = veorq_u8(x11, key_vec);
    x15 = veorq_u8(x15, key_vec);
    x3 = veorq_u8(x3, key_vec);
    x7 = veorq_u8(x7, key_vec);

    // Write output: now try C's write order (x7,x6,...,x0,x15,x14,...,x8)
    vst1q_u8(output + 0 * 16, x7);
    vst1q_u8(output + 1 * 16, x6);
    vst1q_u8(output + 2 * 16, x5);
    vst1q_u8(output + 3 * 16, x4);
    vst1q_u8(output + 4 * 16, x3);
    vst1q_u8(output + 5 * 16, x2);
    vst1q_u8(output + 6 * 16, x1);
    vst1q_u8(output + 7 * 16, x0);
    vst1q_u8(output + 8 * 16, x15);
    vst1q_u8(output + 9 * 16, x14);
    vst1q_u8(output + 10 * 16, x13);
    vst1q_u8(output + 11 * 16, x12);
    vst1q_u8(output + 12 * 16, x11);
    vst1q_u8(output + 13 * 16, x10);
    vst1q_u8(output + 14 * 16, x9);
    vst1q_u8(output + 15 * 16, x8);

    // Check
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (input[i] != output[i]) {
            errors++;
        }
    }

    printf("Result: %d/256 bytes match\n", 256 - errors);

    if (errors > 0) {
        printf("\nFirst 4 blocks:\n");
        for (int b = 0; b < 4; b++) {
            printf("  Block %d:\n    Input:  ", b);
            for (int i = 0; i < 16; i++) printf("%02x", input[b*16+i]);
            printf("\n    Output: ");
            for (int i = 0; i < 16; i++) printf("%02x", output[b*16+i]);
            printf("\n");
        }
    }

    printf("%s\n\n", errors == 0 ? "✓ C test passed" : "✗ C test failed");

    return errors == 0 ? 0 : 1;
}
