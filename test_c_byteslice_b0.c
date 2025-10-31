#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>

// 最小测试：验证C byteslice是否真的破坏b0
int main() {
    uint8x16_t a0, b0, c0, d0, a1, b1, c1, d1, d2, d3;
    uint8x16_t st0_mem, st1_mem;  // 模拟st0, st1

    // 初始化：每个vector都是递增值
    uint8_t init[16];
    for (int i = 0; i < 16; i++) init[i] = i;

    a0 = vld1q_u8(init);
    b0 = vld1q_u8(init);  // b0 = 00 01 02 ... 0f
    c0 = vld1q_u8(init);
    d0 = vld1q_u8(init);
    a1 = vld1q_u8(init);
    b1 = vld1q_u8(init);
    c1 = vld1q_u8(init);
    d1 = vld1q_u8(init);
    d2 = vld1q_u8(init);
    d3 = vld1q_u8(init);

    printf("b0初始值: %02x %02x %02x %02x\n",
           vgetq_lane_u8(b0, 0), vgetq_lane_u8(b0, 1),
           vgetq_lane_u8(b0, 2), vgetq_lane_u8(b0, 3));

    // 模拟Phase 3 Group 0 transpose（简化，只打印不实际transpose）
    printf("\n执行Group 0 transpose(a0, b0, c0, d0, d2, d3)...\n");
    // transpose会修改b0，假设变成: 10 11 12 13
    uint8_t new_b0[16] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                          0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    b0 = vld1q_u8(new_b0);
    printf("b0 transpose后: %02x %02x %02x %02x\n",
           vgetq_lane_u8(b0, 0), vgetq_lane_u8(b0, 1),
           vgetq_lane_u8(b0, 2), vgetq_lane_u8(b0, 3));

    // 模拟Phase 3: st0 = d2
    st0_mem = d2;
    printf("\nvmovdqa128(st0, d2): st0 = d2\n");

    // 模拟Phase 3: b0 = st0
    printf("vmovdqa128(b0, st0): b0 = st0\n");
    b0 = st0_mem;
    printf("b0被覆盖为: %02x %02x %02x %02x\n",
           vgetq_lane_u8(b0, 0), vgetq_lane_u8(b0, 1),
           vgetq_lane_u8(b0, 2), vgetq_lane_u8(b0, 3));

    printf("\n结论：b0被破坏了！不再是transpose后的正确值(10 11 12 13)\n");
    printf("      而是d2的值(00 01 02 03)\n");

    return 0;
}
