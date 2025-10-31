/*
 * test_02_transpose.c - 测试4x4转置操作
 *
 * 目标：验证transpose_4x4宏的正确性，这是byte-slicing的核心操作
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

// 宏定义
#define __m128i uint64x2_t

#define vpunpckhdq128(a, b, o)  (o = (__m128i)vzip2q_u32((uint32x4_t)b, (uint32x4_t)a))
#define vpunpckldq128(a, b, o)  (o = (__m128i)vzip1q_u32((uint32x4_t)b, (uint32x4_t)a))
#define vpunpckhqdq128(a, b, o) (o = (__m128i)vzip2q_u64(b, a))
#define vpunpcklqdq128(a, b, o) (o = (__m128i)vzip1q_u64(b, a))

// transpose_4x4宏（从Kivilinna的实现中提取）
#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
    vpunpckhdq128(x1, x0, t2); \
    vpunpckldq128(x1, x0, x0); \
    \
    vpunpckldq128(x3, x2, t1); \
    vpunpckhdq128(x3, x2, x2); \
    \
    vpunpckhqdq128(t1, x0, x1); \
    vpunpcklqdq128(t1, x0, x0); \
    \
    vpunpckhqdq128(x2, t2, x3); \
    vpunpcklqdq128(x2, t2, x2);

// 辅助函数
void print_m128i_bytes(const char *name, __m128i v) {
    uint8_t bytes[16];
    vst1q_u8(bytes, (uint8x16_t)v);

    printf("%s: ", name);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", bytes[i]);
        if (i % 4 == 3) printf(" ");
    }
    printf("\n");
}

void print_m128i_u32(const char *name, __m128i v) {
    uint32_t words[4];
    vst1q_u32(words, (uint32x4_t)v);
    printf("%s: [%08x] [%08x] [%08x] [%08x]\n",
           name, words[0], words[1], words[2], words[3]);
}

__m128i make_from_u32(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
    uint32_t data[4] = {w0, w1, w2, w3};
    return (__m128i)vld1q_u32(data);
}

// 测试1：简单的4x4矩阵转置
int test_simple_transpose() {
    printf("\n=== Test 1: Simple 4x4 Transpose ===\n");

    __m128i x0, x1, x2, x3, t1, t2;

    // 创建4x4矩阵（每个32位作为一个元素）
    // 原始矩阵（按行）：
    // Row 0: [00 01 02 03]
    // Row 1: [10 11 12 13]
    // Row 2: [20 21 22 23]
    // Row 3: [30 31 32 33]

    x0 = make_from_u32(0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F);  // Row 0 data
    x1 = make_from_u32(0x10111213, 0x14151617, 0x18191A1B, 0x1C1D1E1F);  // Row 1 data
    x2 = make_from_u32(0x20212223, 0x24252627, 0x28292A2B, 0x2C2D2E2F);  // Row 2 data
    x3 = make_from_u32(0x30313233, 0x34353637, 0x38393A3B, 0x3C3D3E3F);  // Row 3 data

    printf("Before transpose:\n");
    print_m128i_bytes("x0", x0);
    print_m128i_bytes("x1", x1);
    print_m128i_bytes("x2", x2);
    print_m128i_bytes("x3", x3);

    // 执行转置
    transpose_4x4(x0, x1, x2, x3, t1, t2);

    printf("\nAfter transpose:\n");
    print_m128i_bytes("x0", x0);
    print_m128i_bytes("x1", x1);
    print_m128i_bytes("x2", x2);
    print_m128i_bytes("x3", x3);

    // 验证结果
    // 转置后（按行）：
    // x0 should contain bytes from position 0 of each original row
    // x1 should contain bytes from position 1 of each original row
    // etc.

    printf("✅ Simple transpose completed (visual check required)\n");
    return 0;
}

// 测试2：更清晰的转置验证
int test_transpose_verification() {
    printf("\n=== Test 2: Transpose Verification ===\n");

    __m128i x0, x1, x2, x3, t1, t2;

    // 使用更简单的模式：每行4个32位值
    // 行0: [A0, A1, A2, A3]  = 0x00000000, 0x11111111, 0x22222222, 0x33333333
    // 行1: [B0, B1, B2, B3]  = 0x44444444, 0x55555555, 0x66666666, 0x77777777
    // 行2: [C0, C1, C2, C3]  = 0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB
    // 行3: [D0, D1, D2, D3]  = 0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF

    x0 = make_from_u32(0x00000000, 0x11111111, 0x22222222, 0x33333333);
    x1 = make_from_u32(0x44444444, 0x55555555, 0x66666666, 0x77777777);
    x2 = make_from_u32(0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB);
    x3 = make_from_u32(0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF);

    printf("Before transpose (as 32-bit words):\n");
    print_m128i_u32("x0", x0);
    print_m128i_u32("x1", x1);
    print_m128i_u32("x2", x2);
    print_m128i_u32("x3", x3);

    // 保存原始数据用于验证
    uint32_t orig[4][4];
    vst1q_u32(orig[0], (uint32x4_t)x0);
    vst1q_u32(orig[1], (uint32x4_t)x1);
    vst1q_u32(orig[2], (uint32x4_t)x2);
    vst1q_u32(orig[3], (uint32x4_t)x3);

    // 执行转置
    transpose_4x4(x0, x1, x2, x3, t1, t2);

    printf("\nAfter transpose (as 32-bit words):\n");
    print_m128i_u32("x0", x0);
    print_m128i_u32("x1", x1);
    print_m128i_u32("x2", x2);
    print_m128i_u32("x3", x3);

    // 验证转置结果
    uint32_t result[4][4];
    vst1q_u32(result[0], (uint32x4_t)x0);
    vst1q_u32(result[1], (uint32x4_t)x1);
    vst1q_u32(result[2], (uint32x4_t)x2);
    vst1q_u32(result[3], (uint32x4_t)x3);

    int errors = 0;
    printf("\nVerification:\n");
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (result[i][j] != orig[j][i]) {
                printf("❌ Error at [%d][%d]: expected 0x%08x, got 0x%08x\n",
                       i, j, orig[j][i], result[i][j]);
                errors++;
            }
        }
    }

    if (errors == 0) {
        printf("✅ Transpose verification passed!\n");
        printf("   Each result[i][j] == original[j][i]\n");
    } else {
        printf("❌ Transpose verification failed with %d errors\n", errors);
    }

    return errors;
}

// 测试3：字节级转置（模拟byteslice的第一步）
int test_byte_level_transpose() {
    printf("\n=== Test 3: Byte-Level Transpose ===\n");

    __m128i x0, x1, x2, x3, t1, t2;

    // 使用字节模式来模拟4个128位块
    // 块0: A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF
    // 块1: B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF
    // 块2: C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF
    // 块3: D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF

    uint8_t block0[16], block1[16], block2[16], block3[16];
    for (int i = 0; i < 16; i++) {
        block0[i] = 0xA0 + i;
        block1[i] = 0xB0 + i;
        block2[i] = 0xC0 + i;
        block3[i] = 0xD0 + i;
    }

    x0 = (__m128i)vld1q_u8(block0);
    x1 = (__m128i)vld1q_u8(block1);
    x2 = (__m128i)vld1q_u8(block2);
    x3 = (__m128i)vld1q_u8(block3);

    printf("Before transpose (4 blocks):\n");
    print_m128i_bytes("Block 0 (x0)", x0);
    print_m128i_bytes("Block 1 (x1)", x1);
    print_m128i_bytes("Block 2 (x2)", x2);
    print_m128i_bytes("Block 3 (x3)", x3);

    // 执行转置（这会按32位元素转置）
    transpose_4x4(x0, x1, x2, x3, t1, t2);

    printf("\nAfter transpose (32-bit transpose):\n");
    print_m128i_bytes("x0", x0);
    print_m128i_bytes("x1", x1);
    print_m128i_bytes("x2", x2);
    print_m128i_bytes("x3", x3);

    printf("\nExpected pattern:\n");
    printf("x0: [A0 A1 A2 A3] [B0 B1 B2 B3] [C0 C1 C2 C3] [D0 D1 D2 D3]\n");
    printf("x1: [A4 A5 A6 A7] [B4 B5 B6 B7] [C4 C5 C6 C7] [D4 D5 D6 D7]\n");
    printf("etc.\n");

    // 验证第一组32位
    uint8_t result[16];
    vst1q_u8(result, (uint8x16_t)x0);

    int errors = 0;
    uint8_t expected[] = {0xA0, 0xA1, 0xA2, 0xA3,  // 从block0的前4字节
                          0xB0, 0xB1, 0xB2, 0xB3,  // 从block1的前4字节
                          0xC0, 0xC1, 0xC2, 0xC3,  // 从block2的前4字节
                          0xD0, 0xD1, 0xD2, 0xD3}; // 从block3的前4字节

    printf("\nVerifying x0:\n");
    printf("Expected: ");
    for (int i = 0; i < 16; i++) printf("%02x ", expected[i]);
    printf("\n");
    printf("Got:      ");
    for (int i = 0; i < 16; i++) printf("%02x ", result[i]);
    printf("\n");

    if (memcmp(result, expected, 16) != 0) {
        printf("❌ Byte-level transpose verification failed\n");
        errors++;
    } else {
        printf("✅ Byte-level transpose verification passed!\n");
    }

    return errors;
}

int main() {
    printf("========================================\n");
    printf("Transpose 4x4 Test Suite\n");
    printf("========================================\n");

    int errors = 0;

    errors += test_simple_transpose();
    errors += test_transpose_verification();
    errors += test_byte_level_transpose();

    printf("\n========================================\n");
    if (errors == 0) {
        printf("✅ All transpose tests PASSED!\n");
    } else {
        printf("❌ %d test(s) FAILED\n", errors);
    }
    printf("========================================\n");

    return errors;
}
