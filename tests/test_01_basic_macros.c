/*
 * test_01_basic_macros.c - 测试基础NEON宏定义
 *
 * 目标：验证AArch64 NEON intrinsics的基本操作是否正确
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

// 从Kivilinna的实现中提取的宏定义
#define __m128i uint64x2_t

#define vpand128(a, b, o)       (o = vandq_u64(b, a))
#define vpxor128(a, b, o)       (o = veorq_u64(b, a))
#define vpor128(a, b, o)        (o = vorrq_u64(b, a))
#define vpandn128(a, b, o)      (o = vbicq_u64(a, b))

#define vpsrlb128(s, a, o)      (o = (__m128i)vshrq_n_u8((uint8x16_t)a, s))
#define vpsllb128(s, a, o)      (o = (__m128i)vshlq_n_u8((uint8x16_t)a, s))

#define vpshufb128(m, a, o)     (o = (__m128i)vqtbl1q_u8((uint8x16_t)a, (uint8x16_t)m))

#define vmovdqa128(a, o)        (o = a)
#define vmovq128(a, o)          ({ uint64x2_t __tmp = { a, 0 }; o = (__m128i)__tmp; })

// 辅助函数：打印128位向量（以字节形式）
void print_m128i_bytes(const char *name, __m128i v) {
    uint8_t bytes[16];
    vst1q_u8(bytes, (uint8x16_t)v);

    printf("%s: ", name);
    for (int i = 0; i < 16; i++) {
        printf("%02x ", bytes[i]);
        if (i == 7) printf(" ");
    }
    printf("\n");
}

// 辅助函数：打印128位向量（以64位形式）
void print_m128i_u64(const char *name, __m128i v) {
    uint64_t words[2];
    vst1q_u64(words, v);
    printf("%s: 0x%016lx %016lx\n", name, words[0], words[1]);
}

// 辅助函数：创建测试向量
__m128i make_test_vector(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                         uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7,
                         uint8_t b8, uint8_t b9, uint8_t b10, uint8_t b11,
                         uint8_t b12, uint8_t b13, uint8_t b14, uint8_t b15) {
    uint8_t data[16] = {b0, b1, b2, b3, b4, b5, b6, b7,
                        b8, b9, b10, b11, b12, b13, b14, b15};
    return (__m128i)vld1q_u8(data);
}

// 测试1：基本逻辑运算
int test_basic_logic() {
    printf("\n=== Test 1: Basic Logic Operations ===\n");

    __m128i a, b, result;

    // 创建测试数据
    // a = 0xF0F0F0F0...
    // b = 0xAAAAAAAA...
    a = make_test_vector(0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
                         0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0);
    b = make_test_vector(0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
                         0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA);

    print_m128i_bytes("Input a", a);
    print_m128i_bytes("Input b", b);

    // 测试 AND
    vpand128(a, b, result);
    print_m128i_bytes("a AND b", result);
    // 预期: 0xF0 & 0xAA = 0xA0

    // 测试 XOR
    vpxor128(a, b, result);
    print_m128i_bytes("a XOR b", result);
    // 预期: 0xF0 ^ 0xAA = 0x5A

    // 测试 OR
    vpor128(a, b, result);
    print_m128i_bytes("a OR b", result);
    // 预期: 0xF0 | 0xAA = 0xFA

    // 测试 AND NOT (a AND NOT b)
    vpandn128(a, b, result);
    print_m128i_bytes("b AND NOT a", result);
    // 预期: 0xAA & ~0xF0 = 0xAA & 0x0F = 0x0A

    // 验证结果
    uint8_t expected_and[16];
    uint8_t expected_xor[16];
    uint8_t actual[16];

    memset(expected_and, 0xA0, 16);
    memset(expected_xor, 0x5A, 16);

    vpand128(a, b, result);
    vst1q_u8(actual, (uint8x16_t)result);
    if (memcmp(actual, expected_and, 16) != 0) {
        printf("❌ AND operation failed!\n");
        return 1;
    }

    vpxor128(a, b, result);
    vst1q_u8(actual, (uint8x16_t)result);
    if (memcmp(actual, expected_xor, 16) != 0) {
        printf("❌ XOR operation failed!\n");
        return 1;
    }

    printf("✅ Logic operations passed!\n");
    return 0;
}

// 测试2：位移操作
int test_shift_operations() {
    printf("\n=== Test 2: Shift Operations ===\n");

    __m128i a, result;

    // 测试数据：每个字节是不同的值
    a = make_test_vector(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
                         0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x81);

    print_m128i_bytes("Input", a);

    // 测试右移4位
    vpsrlb128(4, a, result);
    print_m128i_bytes("Right shift 4", result);
    // 预期: 0x80 >> 4 = 0x08, 0x40 >> 4 = 0x04, ...

    // 测试左移4位
    vpsllb128(4, a, result);
    print_m128i_bytes("Left shift 4", result);
    // 预期: 0x80 << 4 = 0x00 (overflow), 0x40 << 4 = 0x00, 0x01 << 4 = 0x10

    // 验证右移
    vpsrlb128(4, a, result);
    uint8_t actual[16];
    vst1q_u8(actual, (uint8x16_t)result);

    if (actual[0] != 0x08 || actual[1] != 0x04 || actual[7] != 0x00) {
        printf("❌ Right shift failed!\n");
        printf("   Expected: 0x08 0x04 ... 0x00\n");
        printf("   Got:      0x%02x 0x%02x ... 0x%02x\n", actual[0], actual[1], actual[7]);
        return 1;
    }

    printf("✅ Shift operations passed!\n");
    return 0;
}

// 测试3：字节shuffle (vpshufb)
int test_shuffle() {
    printf("\n=== Test 3: Byte Shuffle (vpshufb) ===\n");

    __m128i data, mask, result;

    // 数据：0x00, 0x11, 0x22, ..., 0xFF
    data = make_test_vector(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                            0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    print_m128i_bytes("Input data", data);

    // Mask：反转字节顺序 [15, 14, 13, ..., 1, 0]
    mask = make_test_vector(15, 14, 13, 12, 11, 10, 9, 8,
                            7, 6, 5, 4, 3, 2, 1, 0);

    print_m128i_bytes("Shuffle mask", mask);

    vpshufb128(mask, data, result);
    print_m128i_bytes("Shuffled result", result);
    // 预期：字节顺序反转

    // 验证：第一个字节应该是0xFF，最后一个应该是0x00
    uint8_t actual[16];
    vst1q_u8(actual, (uint8x16_t)result);

    if (actual[0] != 0xFF || actual[15] != 0x00) {
        printf("❌ Shuffle failed!\n");
        printf("   Expected first=0xFF, last=0x00\n");
        printf("   Got first=0x%02x, last=0x%02x\n", actual[0], actual[15]);
        return 1;
    }

    // 测试2：提取特定位置 [0, 4, 8, 12, ...]
    mask = make_test_vector(0, 4, 8, 12, 0, 4, 8, 12,
                            0, 4, 8, 12, 0, 4, 8, 12);
    print_m128i_bytes("Extract mask", mask);

    vpshufb128(mask, data, result);
    print_m128i_bytes("Extracted result", result);
    // 预期：重复 0x00, 0x44, 0x88, 0xCC

    vst1q_u8(actual, (uint8x16_t)result);
    if (actual[0] != 0x00 || actual[1] != 0x44 || actual[2] != 0x88 || actual[3] != 0xCC) {
        printf("❌ Extract shuffle failed!\n");
        return 1;
    }

    printf("✅ Shuffle operations passed!\n");
    return 0;
}

// 测试4：模拟filter_8bit的基础操作
int test_filter_8bit_basics() {
    printf("\n=== Test 4: filter_8bit Basics ===\n");

    __m128i x, mask4, lo_part, hi_part;
    __m128i lo_table, hi_table, lo_result, hi_result, final;

    // 测试数据
    x = make_test_vector(0x5A, 0x3C, 0xF0, 0x0F, 0xAA, 0x55, 0x12, 0x34,
                         0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF1, 0x23, 0x45);

    print_m128i_bytes("Input x", x);

    // 创建0x0F mask
    mask4 = make_test_vector(0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                             0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F);

    // 步骤1：提取低4位
    vpand128(x, mask4, lo_part);
    print_m128i_bytes("Low 4 bits", lo_part);

    // 步骤2：提取高4位（右移4位）
    vpsrlb128(4, x, hi_part);
    print_m128i_bytes("High 4 bits", hi_part);

    // 创建简单的查找表（恒等映射）
    lo_table = make_test_vector(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F);
    hi_table = make_test_vector(0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
                                0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);

    print_m128i_bytes("Lo table", lo_table);
    print_m128i_bytes("Hi table", hi_table);

    // 步骤3：查表
    vpshufb128(lo_part, lo_table, lo_result);
    vpshufb128(hi_part, hi_table, hi_result);

    print_m128i_bytes("Lo lookup result", lo_result);
    print_m128i_bytes("Hi lookup result", hi_result);

    // 步骤4：合并（XOR）
    vpxor128(lo_result, hi_result, final);
    print_m128i_bytes("Final (lo XOR hi)", final);

    // 验证：0x5A = 0101 1010 = hi:5, lo:A
    // lo_table[0xA] = 0x0A
    // hi_table[0x5] = 0x50
    // 0x0A ^ 0x50 = 0x5A (恒等)
    uint8_t actual[16];
    vst1q_u8(actual, (uint8x16_t)final);

    uint8_t expected[16];
    vst1q_u8(expected, (uint8x16_t)x);

    if (memcmp(actual, expected, 16) == 0) {
        printf("✅ filter_8bit basics (identity transform) passed!\n");
        return 0;
    } else {
        printf("❌ filter_8bit basics failed!\n");
        printf("   Expected identity transform\n");
        return 1;
    }
}

int main() {
    printf("========================================\n");
    printf("NEON Basic Macros Test Suite\n");
    printf("========================================\n");

    int errors = 0;

    errors += test_basic_logic();
    errors += test_shift_operations();
    errors += test_shuffle();
    errors += test_filter_8bit_basics();

    printf("\n========================================\n");
    if (errors == 0) {
        printf("✅ All basic tests PASSED!\n");
    } else {
        printf("❌ %d test(s) FAILED\n", errors);
    }
    printf("========================================\n");

    return errors;
}
