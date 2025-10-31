/*
 * C reference for single roundsm16 - direct comparison with assembly
 * Takes byte-sliced AB/CD input, runs ONE roundsm16, outputs CD
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

void print_state(const char *label, const uint8_t *state) {
    printf("%s:\n", label);
    for (int i = 0; i < 8; i++) {
        printf("  [%d]: ", i);
        for (int j = 0; j < 16; j++) {
            if (j > 0 && j % 4 == 0) printf(" ");
            printf("%02x", state[i * 16 + j]);
        }
        printf("\n");
    }
}

// Manual roundsm16 implementation using NEON intrinsics
void roundsm16_c_reference(uint8x16_t ab[8], uint8x16_t cd[8], uint64_t key[4]) {
    uint8x16_t x[8];

    // Copy AB to x for processing
    for (int i = 0; i < 8; i++) {
        x[i] = ab[i];
    }

    // Phase 1: Inverse ShiftRows
    const uint8_t inv_shift_row[16] = {
        0, 13, 10, 7, 4, 1, 14, 11, 8, 5, 2, 15, 12, 9, 6, 3
    };
    uint8x16_t inv_shift_tbl = vld1q_u8(inv_shift_row);
    for (int i = 0; i < 8; i++) {
        x[i] = vqtbl1q_u8(x[i], inv_shift_tbl);
    }

    // Phase 2: Pre-filter (bitmatrix transformation)
    // This is complex - using the actual pre-filter from the C implementation
    extern void apply_pre_filter(uint8x16_t *x);
    apply_pre_filter(x);

    // Phase 3: SubBytes using AESE
    uint8x16_t zero = vdupq_n_u8(0);
    for (int i = 0; i < 8; i++) {
        x[i] = vaeseq_u8(x[i], zero);
    }

    // Phase 4: Post-filter (bitmatrix transformation)
    extern void apply_post_filter(uint8x16_t *x);
    apply_post_filter(x);

    // Phase 5: P-function (16 XORs for diffusion)
    uint8x16_t t[8];
    t[0] = veorq_u8(x[0], x[1]);
    t[2] = veorq_u8(x[2], x[3]);
    t[4] = veorq_u8(x[4], x[5]);
    t[6] = veorq_u8(x[6], x[7]);

    t[1] = veorq_u8(t[0], t[2]);
    t[5] = veorq_u8(t[4], t[6]);
    t[3] = veorq_u8(t[1], t[5]);

    t[7] = veorq_u8(t[6], t[3]);
    t[6] = veorq_u8(t[5], t[3]);
    t[5] = veorq_u8(t[4], t[3]);
    t[4] = veorq_u8(x[5], t[3]);

    t[2] = veorq_u8(t[1], t[3]);
    t[1] = veorq_u8(t[0], t[3]);
    t[0] = veorq_u8(x[1], t[3]);

    x[0] = veorq_u8(x[0], t[3]);
    x[1] = t[0];
    x[2] = veorq_u8(x[2], t[3]);
    x[3] = t[2];
    x[4] = veorq_u8(x[4], t[3]);
    x[5] = t[4];
    x[6] = veorq_u8(x[6], t[3]);
    x[7] = t[7];

    // Phase 6: Key addition (XOR with key[k+2..k+4]) and Feistel XOR with CD
    uint8x16_t k0 = vreinterpretq_u8_u64(vdupq_n_u64(key[0]));
    uint8x16_t k1 = vreinterpretq_u8_u64(vdupq_n_u64(key[1]));

    // XOR with key and then XOR with old CD to get new CD
    cd[0] = veorq_u8(veorq_u8(x[0], k0), cd[0]);
    cd[1] = veorq_u8(veorq_u8(x[1], k0), cd[1]);
    cd[2] = veorq_u8(veorq_u8(x[2], k1), cd[2]);
    cd[3] = veorq_u8(veorq_u8(x[3], k1), cd[3]);
    cd[4] = veorq_u8(veorq_u8(x[4], k0), cd[4]);
    cd[5] = veorq_u8(veorq_u8(x[5], k0), cd[5]);
    cd[6] = veorq_u8(veorq_u8(x[6], k1), cd[6]);
    cd[7] = veorq_u8(veorq_u8(x[7], k1), cd[7]);
}

int main(void) {
    struct camellia_simd_ctx ctx;

    // Create simple byte-sliced AB state
    uint8x16_t ab[8], cd[8];
    uint8_t ab_buf[128], cd_buf[128];

    printf("========================================\n");
    printf("C Reference: Single Roundsm16 Test\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    printf("Testing with k=2 (key_table[2] = %016lx)\n\n", ctx.key_table[2]);

    // Create same input as assembly test
    for (int i = 0; i < 8; i++) {
        ab[i] = vdupq_n_u8(i * 0x11);
    }
    for (int i = 0; i < 8; i++) {
        cd[i] = vdupq_n_u8(0);
    }

    printf("Input AB state (byte-sliced):\n");
    for (int i = 0; i < 8; i++) {
        vst1q_u8(&ab_buf[i * 16], ab[i]);
    }
    print_state("  AB", ab_buf);

    printf("\nInput CD state (all zeros):\n");
    printf("  CD[0]: ");
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("00");
    }
    printf("\n\n");

    // Run single roundsm16 - use key_table[2] and key_table[3]
    uint64_t key[4];
    key[0] = ctx.key_table[2];
    key[1] = ctx.key_table[3];
    key[2] = ctx.key_table[2];  // Duplicate for x4,x5 vectors
    key[3] = ctx.key_table[3];  // Duplicate for x6,x7 vectors

    roundsm16_c_reference(ab, cd, key);

    printf("C Reference Output CD state:\n");
    for (int i = 0; i < 8; i++) {
        vst1q_u8(&cd_buf[i * 16], cd[i]);
    }
    print_state("  CD", cd_buf);

    return 0;
}
