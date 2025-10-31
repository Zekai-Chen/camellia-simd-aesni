/*
 * Standalone C implementation of roundsm16 for wrapper
 *
 * TODO: Fix Assembly roundsm16 and remove this file
 */

#define CAMELLIA_SIMD128_USE_AES_INSTRUCTION_SET
#include "camellia_simd.h"
#include <arm_neon.h>
#include <stdint.h>

// Pull in all the necessary macros and constants
#include "camellia_simd128_with_aes_instruction_set.c"

/*
 * roundsm16_wrapper_aarch64
 *
 * C wrapper that calls the C roundsm16 macro
 */
void roundsm16_wrapper_aarch64(uint8_t *ab_bytes, uint8_t *cd_bytes, uint64_t key)
{
    // Local stack variables needed by macros
    __m128i inv_shift_row_stack, mask_0f_stack, shufb_16x16b_stack, pack_bswap_stack;
    __m128i pre_tf_lo_s1_stack, pre_tf_hi_s1_stack;
    __m128i pre_tf_lo_s4_stack, pre_tf_hi_s4_stack;
    __m128i post_tf_lo_s1_stack, post_tf_hi_s1_stack;
    __m128i post_tf_lo_s2_stack, post_tf_hi_s2_stack;
    __m128i post_tf_lo_s3_stack, post_tf_hi_s3_stack;
    __m128i bcast[8];

    // Load stack constants
    vmovdqa128(vmovq64_to_128(inv_shift_row), inv_shift_row_stack);
    vmovdqa128(vmovq64_to_128(mask_0f), mask_0f_stack);
    vmovdqa128(vmovq64_to_128(shufb_16x16b), shufb_16x16b_stack);
    vmovdqa128(vmovq64_to_128(pack_bswap), pack_bswap_stack);

    vmovdqa128(vmovq64_to_128(pre_tf_lo_s1), pre_tf_lo_s1_stack);
    vmovdqa128(vmovq64_to_128(pre_tf_hi_s1), pre_tf_hi_s1_stack);
    vmovdqa128(vmovq64_to_128(pre_tf_lo_s4), pre_tf_lo_s4_stack);
    vmovdqa128(vmovq64_to_128(pre_tf_hi_s4), pre_tf_hi_s4_stack);
    vmovdqa128(vmovq64_to_128(post_tf_lo_s1), post_tf_lo_s1_stack);
    vmovdqa128(vmovq64_to_128(post_tf_hi_s1), post_tf_hi_s1_stack);
    vmovdqa128(vmovq64_to_128(post_tf_lo_s2), post_tf_lo_s2_stack);
    vmovdqa128(vmovq64_to_128(post_tf_hi_s2), post_tf_hi_s2_stack);
    vmovdqa128(vmovq64_to_128(post_tf_lo_s3), post_tf_lo_s3_stack);
    vmovdqa128(vmovq64_to_128(post_tf_hi_s3), post_tf_hi_s3_stack);

    // Load broadcast patterns
    for (int i = 0; i < 8; i++) {
        bcast[i] = vdupq_n_u8(i);
    }

    // Load AB and CD states
    __m128i x0, x1, x2, x3, x4, x5, x6, x7;
    __m128i y0, y1, y2, y3, y4, y5, y6, y7;
    __m128i mem_cd[8];

    vmovdqa128(*(__m128i*)&ab_bytes[0], x0);
    vmovdqa128(*(__m128i*)&ab_bytes[16], x1);
    vmovdqa128(*(__m128i*)&ab_bytes[32], x2);
    vmovdqa128(*(__m128i*)&ab_bytes[48], x3);
    vmovdqa128(*(__m128i*)&ab_bytes[64], x4);
    vmovdqa128(*(__m128i*)&ab_bytes[80], x5);
    vmovdqa128(*(__m128i*)&ab_bytes[96], x6);
    vmovdqa128(*(__m128i*)&ab_bytes[112], x7);

    vmovdqa128(*(__m128i*)&cd_bytes[0], mem_cd[0]);
    vmovdqa128(*(__m128i*)&cd_bytes[16], mem_cd[1]);
    vmovdqa128(*(__m128i*)&cd_bytes[32], mem_cd[2]);
    vmovdqa128(*(__m128i*)&cd_bytes[48], mem_cd[3]);
    vmovdqa128(*(__m128i*)&cd_bytes[64], mem_cd[4]);
    vmovdqa128(*(__m128i*)&cd_bytes[80], mem_cd[5]);
    vmovdqa128(*(__m128i*)&cd_bytes[96], mem_cd[6]);
    vmovdqa128(*(__m128i*)&cd_bytes[112], mem_cd[7]);

    // Call roundsm16 macro
    roundsm16(x0, x1, x2, x3, x4, x5, x6, x7,
              y0, y1, y2, y3, y4, y5, y6, y7,
              mem_cd, key);

    // Store result in expected order: x4, x5, x6, x7, x0, x1, x2, x3
    vmovdqa128(x4, *(__m128i*)&ab_bytes[0]);
    vmovdqa128(x5, *(__m128i*)&ab_bytes[16]);
    vmovdqa128(x6, *(__m128i*)&ab_bytes[32]);
    vmovdqa128(x7, *(__m128i*)&ab_bytes[48]);
    vmovdqa128(x0, *(__m128i*)&ab_bytes[64]);
    vmovdqa128(x1, *(__m128i*)&ab_bytes[80]);
    vmovdqa128(x2, *(__m128i*)&ab_bytes[96]);
    vmovdqa128(x3, *(__m128i*)&ab_bytes[112]);
}
