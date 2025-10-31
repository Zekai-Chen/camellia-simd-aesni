/*
 * Test Assembly transpose_4x4 macro
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

// C reference transpose
void transpose_4x4_c(uint8x16_t *x0, uint8x16_t *x1, uint8x16_t *x2, uint8x16_t *x3) {
    uint32x4_t _x0 = vreinterpretq_u32_u8(*x0);
    uint32x4_t _x1 = vreinterpretq_u32_u8(*x1);
    uint32x4_t _x2 = vreinterpretq_u32_u8(*x2);
    uint32x4_t _x3 = vreinterpretq_u32_u8(*x3);
    uint32x4_t _t2, _t1;
    uint64x2_t __x0, __x1, __x2, __x3, __t1, __t2;

    _t2 = vzip2q_u32(_x0, _x1);
    _x0 = vzip1q_u32(_x0, _x1);
    _t1 = vzip1q_u32(_x2, _x3);
    _x2 = vzip2q_u32(_x2, _x3);

    __x0 = vreinterpretq_u64_u32(_x0);
    __t1 = vreinterpretq_u64_u32(_t1);
    __x1 = vzip2q_u64(__x0, __t1);
    __x0 = vzip1q_u64(__x0, __t1);

    __t2 = vreinterpretq_u64_u32(_t2);
    __x2 = vreinterpretq_u64_u32(_x2);
    __x3 = vzip2q_u64(__t2, __x2);
    __x2 = vzip1q_u64(__t2, __x2);

    *x0 = vreinterpretq_u8_u64(__x0);
    *x1 = vreinterpretq_u8_u64(__x1);
    *x2 = vreinterpretq_u8_u64(__x2);
    *x3 = vreinterpretq_u8_u64(__x3);
}

// Assembly version
extern void test_asm_transpose_4x4(uint8_t *data);

int main() {
    uint8_t data_c[64], data_asm[64];

    // Initialize test data: blocks 0-3 with distinctive patterns
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            data_c[i * 16 + j] = i * 16 + j;
            data_asm[i * 16 + j] = i * 16 + j;
        }
    }

    printf("Input (4 blocks):\n");
    for (int i = 0; i < 4; i++) {
        printf("  Block %d: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", data_c[i * 16 + j]);
        printf("\n");
    }

    // C reference
    uint8x16_t c0 = vld1q_u8(&data_c[0]);
    uint8x16_t c1 = vld1q_u8(&data_c[16]);
    uint8x16_t c2 = vld1q_u8(&data_c[32]);
    uint8x16_t c3 = vld1q_u8(&data_c[48]);
    transpose_4x4_c(&c0, &c1, &c2, &c3);
    vst1q_u8(&data_c[0], c0);
    vst1q_u8(&data_c[16], c1);
    vst1q_u8(&data_c[32], c2);
    vst1q_u8(&data_c[48], c3);

    // Assembly
    test_asm_transpose_4x4(data_asm);

    printf("\nC reference output:\n");
    for (int i = 0; i < 4; i++) {
        printf("  Vector %d: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", data_c[i * 16 + j]);
        printf("\n");
    }

    printf("\nAssembly output:\n");
    for (int i = 0; i < 4; i++) {
        printf("  Vector %d: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", data_asm[i * 16 + j]);
        printf("\n");
    }

    // Compare
    int match = 1;
    for (int i = 0; i < 64; i++) {
        if (data_c[i] != data_asm[i]) {
            printf("\nMISMATCH at byte %d: C=%02x, ASM=%02x\n", i, data_c[i], data_asm[i]);
            match = 0;
            break;
        }
    }

    if (match) {
        printf("\nSUCCESS: Assembly transpose matches C reference!\n");
        return 0;
    } else {
        printf("\nFAILURE: Assembly transpose differs from C reference!\n");
        return 1;
    }
}
