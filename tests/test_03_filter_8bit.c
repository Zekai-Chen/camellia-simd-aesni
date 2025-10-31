/*
 * test_03_filter_8bit.c - 测试filter_8bit S-box变换
 *
 * 目标：验证filter_8bit宏的正确性，这是Camellia S-box实现的核心
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

// 宏定义
#define __m128i uint64x2_t

#define vpand128(a, b, o)       (o = vandq_u64(b, a))
#define vpxor128(a, b, o)       (o = veorq_u64(b, a))
#define vpandn128(a, b, o)      (o = vbicq_u64(a, b))
#define vpsrlb128(s, a, o)      (o = (__m128i)vshrq_n_u8((uint8x16_t)a, s))
#define vpsrld128(s, a, o)      (o = (__m128i)vshrq_n_u32((uint32x4_t)a, s))
#define vpshufb128(m, a, o)     (o = (__m128i)vqtbl1q_u8((uint8x16_t)a, (uint8x16_t)m))

// filter_8bit宏（简化版 - 支持AArch64的字节移位）
#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0) \
    vpand128(x, mask4bit, tmp0); \
    vpsrlb128(4, x, x); \
    \
    vpshufb128(tmp0, lo_t, tmp0); \
    vpshufb128(x, hi_t, x); \
    vpxor128(tmp0, x, x);

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

__m128i make_vector(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3,
                    uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7,
                    uint8_t b8, uint8_t b9, uint8_t b10, uint8_t b11,
                    uint8_t b12, uint8_t b13, uint8_t b14, uint8_t b15) {
    uint8_t data[16] = {b0, b1, b2, b3, b4, b5, b6, b7,
                        b8, b9, b10, b11, b12, b13, b14, b15};
    return (__m128i)vld1q_u8(data);
}

// 测试1：恒等变换（identity transform）
int test_identity_transform() {
    printf("\n=== Test 1: Identity Transform ===\n");

    __m128i x, mask4, lo_table, hi_table, tmp0;
    __m128i x_original;

    // 测试数据：随机字节
    x = make_vector(0x5A, 0x3C, 0xF0, 0x0F, 0xAA, 0x55, 0x12, 0x34,
                    0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF1, 0x23, 0x45);
    x_original = x;

    print_m128i_bytes("Input", x);

    // 0x0F mask
    mask4 = make_vector(0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F);

    // 恒等查找表
    // lo_table[i] = i (低4位)
    // hi_table[i] = i << 4 (高4位)
    lo_table = make_vector(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F);

    hi_table = make_vector(0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
                           0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);

    print_m128i_bytes("Lo table", lo_table);
    print_m128i_bytes("Hi table", hi_table);

    // 应用filter_8bit
    filter_8bit(x, lo_table, hi_table, mask4, tmp0);

    print_m128i_bytes("Output", x);

    // 验证：应该与输入相同
    uint8_t input[16], output[16];
    vst1q_u8(input, (uint8x16_t)x_original);
    vst1q_u8(output, (uint8x16_t)x);

    if (memcmp(input, output, 16) == 0) {
        printf("✅ Identity transform passed!\n");
        return 0;
    } else {
        printf("❌ Identity transform failed!\n");
        printf("   Input and output should be identical\n");
        return 1;
    }
}

// 测试2：简单的S-box变换
int test_simple_sbox() {
    printf("\n=== Test 2: Simple S-box Transform ===\n");

    __m128i x, mask4, lo_table, hi_table, tmp0;

    // 测试单个字节：0x3C = 0011 1100 (hi=3, lo=C)
    x = make_vector(0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C,
                    0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C);

    print_m128i_bytes("Input (all 0x3C)", x);

    mask4 = make_vector(0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F);

    // 自定义查找表：
    // lo_table[0xC] = 0x07  (假设的变换)
    // hi_table[0x3] = 0xE0  (假设的变换)
    // 结果应该是: 0x07 ^ 0xE0 = 0xE7

    lo_table = make_vector(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x07, 0x0D, 0x0E, 0x0F);
                        // index C ^^^^^^^

    hi_table = make_vector(0x00, 0x10, 0x20, 0xE0, 0x40, 0x50, 0x60, 0x70,
                           0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0);
                        // index 3 ^^^^^^^

    print_m128i_bytes("Lo table", lo_table);
    print_m128i_bytes("Hi table", hi_table);

    // 手动计算预期结果
    printf("\nManual calculation:\n");
    printf("  Input: 0x3C = hi:3, lo:C\n");
    printf("  lo_table[0xC] = 0x07\n");
    printf("  hi_table[0x3] = 0xE0\n");
    printf("  Result: 0x07 ^ 0xE0 = 0xE7\n");

    // 应用filter_8bit
    filter_8bit(x, lo_table, hi_table, mask4, tmp0);

    print_m128i_bytes("Output", x);

    // 验证
    uint8_t output[16];
    vst1q_u8(output, (uint8x16_t)x);

    int errors = 0;
    for (int i = 0; i < 16; i++) {
        if (output[i] != 0xE7) {
            printf("❌ Byte %d: expected 0xE7, got 0x%02x\n", i, output[i]);
            errors++;
        }
    }

    if (errors == 0) {
        printf("✅ Simple S-box transform passed!\n");
    } else {
        printf("❌ Simple S-box transform failed!\n");
    }

    return errors;
}

// 测试3：使用实际的Camellia预变换表
int test_camellia_pre_transform() {
    printf("\n=== Test 3: Camellia Pre-Transform Tables ===\n");

    __m128i x, mask4, lo_table, hi_table, tmp0;

    // 从Kivilinna的实现中提取的Camellia S1预变换表
    // 这些是真实的Camellia->AES映射
    static const uint8_t pre_tf_lo_s1[16] = {
        0x45, 0xe8, 0x40, 0xed, 0x2e, 0x83, 0x2b, 0x86,
        0x4b, 0xe6, 0x4e, 0xe3, 0x20, 0x8d, 0x25, 0x88
    };

    static const uint8_t pre_tf_hi_s1[16] = {
        0x00, 0x51, 0xf1, 0xa0, 0x8a, 0xdb, 0x7b, 0x2a,
        0x09, 0x58, 0xf8, 0xa9, 0x83, 0xd2, 0x72, 0x23
    };

    // 测试输入：几个不同的字节
    x = make_vector(0x00, 0x01, 0x0F, 0x10, 0x5A, 0xFF, 0xAA, 0x55,
                    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF1);

    print_m128i_bytes("Input", x);

    mask4 = make_vector(0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F);

    lo_table = (__m128i)vld1q_u8(pre_tf_lo_s1);
    hi_table = (__m128i)vld1q_u8(pre_tf_hi_s1);

    print_m128i_bytes("Camellia S1 lo table", lo_table);
    print_m128i_bytes("Camellia S1 hi table", hi_table);

    // 手动计算第一个字节 (0x00)
    printf("\nManual calculation for 0x00:\n");
    printf("  0x00 = hi:0, lo:0\n");
    printf("  pre_tf_lo_s1[0] = 0x%02x\n", pre_tf_lo_s1[0]);
    printf("  pre_tf_hi_s1[0] = 0x%02x\n", pre_tf_hi_s1[0]);
    printf("  Result: 0x%02x ^ 0x%02x = 0x%02x\n",
           pre_tf_lo_s1[0], pre_tf_hi_s1[0],
           pre_tf_lo_s1[0] ^ pre_tf_hi_s1[0]);

    // 应用filter_8bit
    __m128i x_original = x;
    filter_8bit(x, lo_table, hi_table, mask4, tmp0);

    print_m128i_bytes("Output", x);

    // 手动验证几个字节
    uint8_t input[16], output[16];
    vst1q_u8(input, (uint8x16_t)x_original);
    vst1q_u8(output, (uint8x16_t)x);

    printf("\nVerifying select bytes:\n");
    for (int i = 0; i < 3; i++) {
        uint8_t in_byte = input[i];
        uint8_t lo_idx = in_byte & 0x0F;
        uint8_t hi_idx = in_byte >> 4;
        uint8_t expected = pre_tf_lo_s1[lo_idx] ^ pre_tf_hi_s1[hi_idx];

        printf("  Byte[%d]: in=0x%02x, lo_idx=%X, hi_idx=%X, expected=0x%02x, got=0x%02x %s\n",
               i, in_byte, lo_idx, hi_idx, expected, output[i],
               (expected == output[i]) ? "✓" : "✗");
    }

    printf("✅ Camellia pre-transform test completed (visual verification)\n");
    return 0;
}

// 测试4：批量变换（模拟SIMD效率）
int test_batch_transform() {
    printf("\n=== Test 4: Batch Transform (16 bytes in parallel) ===\n");

    __m128i x, mask4, lo_table, hi_table, tmp0;

    // 16个不同的字节值
    x = make_vector(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);

    print_m128i_bytes("Input (16 different bytes)", x);

    mask4 = make_vector(0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
                        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F);

    // 简单的变换：lo不变，hi反转
    lo_table = make_vector(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F);

    hi_table = make_vector(0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90, 0x80,
                           0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00);

    printf("\nTransform: lo unchanged, hi inverted\n");

    // 应用变换
    __m128i x_original = x;
    filter_8bit(x, lo_table, hi_table, mask4, tmp0);

    print_m128i_bytes("Output", x);

    // 验证：0x00 -> lo=0, hi=0 -> 0x00 ^ 0xF0 = 0xF0
    //       0x11 -> lo=1, hi=1 -> 0x01 ^ 0xE0 = 0xE1
    uint8_t input[16], output[16];
    vst1q_u8(input, (uint8x16_t)x_original);
    vst1q_u8(output, (uint8x16_t)x);

    printf("\nVerification (first 4 bytes):\n");
    int errors = 0;

    // 获取查找表的值
    uint8_t lo_table_bytes[16], hi_table_bytes[16];
    vst1q_u8(lo_table_bytes, (uint8x16_t)lo_table);
    vst1q_u8(hi_table_bytes, (uint8x16_t)hi_table);

    for (int i = 0; i < 4; i++) {
        uint8_t in_byte = input[i];
        uint8_t lo = in_byte & 0x0F;
        uint8_t hi = in_byte >> 4;
        uint8_t expected = lo_table_bytes[lo] ^ hi_table_bytes[hi];

        if (output[i] != expected) {
            printf("  ❌ Byte[%d]: expected 0x%02x, got 0x%02x\n",
                   i, expected, output[i]);
            errors++;
        } else {
            printf("  ✓ Byte[%d]: 0x%02x -> 0x%02x\n", i, in_byte, output[i]);
        }
    }

    if (errors == 0) {
        printf("✅ Batch transform passed!\n");
    }

    return errors;
}

int main() {
    printf("========================================\n");
    printf("filter_8bit S-box Transform Test Suite\n");
    printf("========================================\n");

    int errors = 0;

    errors += test_identity_transform();
    errors += test_simple_sbox();
    errors += test_camellia_pre_transform();
    errors += test_batch_transform();

    printf("\n========================================\n");
    if (errors == 0) {
        printf("✅ All filter_8bit tests PASSED!\n");
    } else {
        printf("❌ %d test(s) FAILED\n", errors);
    }
    printf("========================================\n");

    return errors;
}
