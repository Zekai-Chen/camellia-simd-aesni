#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// 测试数据：16个相同的block，便于观察byteslice结果
static uint8_t test_input[256];

// 初始化：每个block都是递增序列 00 01 02 ... 0f
void init_test_input() {
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            test_input[i * 16 + j] = j;  // 每个block: 00 01 02 03 ... 0f
        }
    }
}

// C参考实现：只做Phase 1（4x4 transpose）
void c_byteslice_phase1_only(uint8_t output[256]) {
    uint8x16_t a0, a1, a2, a3, b0, b1, b2, b3;
    uint8x16_t c0, c1, c2, c3, d0, d1, d2, d3;
    uint8x16_t t0, t1;

    // Load 16 blocks
    a0 = vld1q_u8(&test_input[0]);
    b0 = vld1q_u8(&test_input[16]);
    c0 = vld1q_u8(&test_input[32]);
    d0 = vld1q_u8(&test_input[48]);
    a1 = vld1q_u8(&test_input[64]);
    b1 = vld1q_u8(&test_input[80]);
    c1 = vld1q_u8(&test_input[96]);
    d1 = vld1q_u8(&test_input[112]);
    a2 = vld1q_u8(&test_input[128]);
    b2 = vld1q_u8(&test_input[144]);
    c2 = vld1q_u8(&test_input[160]);
    d2 = vld1q_u8(&test_input[176]);
    a3 = vld1q_u8(&test_input[192]);
    b3 = vld1q_u8(&test_input[208]);
    c3 = vld1q_u8(&test_input[224]);
    d3 = vld1q_u8(&test_input[240]);

    printf("C Phase 1 输入:\n");
    printf("  a0[0-3]: %02x %02x %02x %02x\n",
           vgetq_lane_u8(a0, 0), vgetq_lane_u8(a0, 1),
           vgetq_lane_u8(a0, 2), vgetq_lane_u8(a0, 3));

    // Phase 1: transpose_4x4 on rows
    // Group 1: a0, a1, a2, a3
    uint32x4_t a0_32 = vreinterpretq_u32_u8(a0);
    uint32x4_t a1_32 = vreinterpretq_u32_u8(a1);
    uint32x4_t a2_32 = vreinterpretq_u32_u8(a2);
    uint32x4_t a3_32 = vreinterpretq_u32_u8(a3);

    uint32x4_t t2_32 = vzip1q_u32(a1_32, a0_32);
    a0_32 = vzip1q_u32(a0_32, a1_32);  // 这里会被修改

    uint32x4_t t1_32 = vzip1q_u32(a3_32, a2_32);
    a2_32 = vzip1q_u32(a2_32, a3_32);

    uint64x2_t a1_64 = vreinterpretq_u64_u32(vzip1q_u32(a2_32, a0_32));
    a0 = vreinterpretq_u8_u64(vzip1q_u64(a1_64, vreinterpretq_u64_u32(a0_32)));

    a1 = vreinterpretq_u8_u64(vzip2q_u64(a1_64, vreinterpretq_u64_u32(a0_32)));

    uint64x2_t a3_64 = vreinterpretq_u64_u32(vzip2q_u32(a2_32, t2_32));
    a2 = vreinterpretq_u8_u64(vzip1q_u64(a3_64, vreinterpretq_u64_u32(t2_32)));
    a3 = vreinterpretq_u8_u64(vzip2q_u64(a3_64, vreinterpretq_u64_u32(t2_32)));

    printf("\nC Phase 1 输出（只做了a0-a3的transpose）:\n");
    printf("  a0[0-3]: %02x %02x %02x %02x\n",
           vgetq_lane_u8(a0, 0), vgetq_lane_u8(a0, 1),
           vgetq_lane_u8(a0, 2), vgetq_lane_u8(a0, 3));
    printf("  a1[0-3]: %02x %02x %02x %02x\n",
           vgetq_lane_u8(a1, 0), vgetq_lane_u8(a1, 1),
           vgetq_lane_u8(a1, 2), vgetq_lane_u8(a1, 3));
    printf("  a2[0-3]: %02x %02x %02x %02x\n",
           vgetq_lane_u8(a2, 0), vgetq_lane_u8(a2, 1),
           vgetq_lane_u8(a2, 2), vgetq_lane_u8(a2, 3));
    printf("  a3[0-3]: %02x %02x %02x %02x\n",
           vgetq_lane_u8(a3, 0), vgetq_lane_u8(a3, 1),
           vgetq_lane_u8(a3, 2), vgetq_lane_u8(a3, 3));

    // Store output
    vst1q_u8(&output[0], a0);
    vst1q_u8(&output[16], a1);
    vst1q_u8(&output[32], a2);
    vst1q_u8(&output[48], a3);
}

int main() {
    uint8_t c_output[256];

    init_test_input();

    printf("========================================\n");
    printf("Byteslice Phase 1 单元测试\n");
    printf("========================================\n\n");

    printf("输入数据：每个block都是 00 01 02 03 ... 0f\n");
    printf("Block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x ", test_input[i]);
    printf("\n\n");

    c_byteslice_phase1_only(c_output);

    printf("\n预期结果：transpose后，a0应该包含每个block的第0个32-bit word\n");
    printf("  a0 = [block3[0-3], block2[0-3], block1[0-3], block0[0-3]]\n");
    printf("     = [00010203, 00010203, 00010203, 00010203]\n");

    return 0;
}
