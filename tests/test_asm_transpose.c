/*
 * test_asm_transpose.c - Test the transpose_4x4 macro in assembly
 *
 * This test verifies that the transpose_4x4 macro implemented in
 * camellia_simd128_aarch64_neon_crypto.S works correctly by comparing
 * its output against the known-good C intrinsics version.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

// Assembly test function
extern void test_transpose_4x4_asm(void *output[4], const void *input[4]);

// Reference C implementation using intrinsics (from camellia_simd128_with_aes_instruction_set.c)
#define transpose_4x4_c(x0, x1, x2, x3, t1, t2) \
    do { \
        t2 = (__m128i)vzip2q_u32((uint32x4_t)x0, (uint32x4_t)x1); \
        x0 = (__m128i)vzip1q_u32((uint32x4_t)x0, (uint32x4_t)x1); \
        t1 = (__m128i)vzip1q_u32((uint32x4_t)x2, (uint32x4_t)x3); \
        x2 = (__m128i)vzip2q_u32((uint32x4_t)x2, (uint32x4_t)x3); \
        x1 = (__m128i)vzip2q_u64(x0, t1); \
        x0 = (__m128i)vzip1q_u64(x0, t1); \
        x3 = (__m128i)vzip2q_u64(t2, x2); \
        x2 = (__m128i)vzip1q_u64(t2, x2); \
    } while (0)

typedef uint64x2_t __m128i;

void print_hex(const char *name, const uint8_t *data, int len) {
    printf("%s: ", name);
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if (i % 4 == 3 && i != len - 1) printf(" ");
    }
    printf("\n");
}

void print_as_u32(const char *name, const uint8_t *data) {
    uint32_t *words = (uint32_t *)data;
    printf("%s: [%08x] [%08x] [%08x] [%08x]\n",
           name, words[0], words[1], words[2], words[3]);
}

__m128i make_from_u32(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
    uint32_t data[4] = {w0, w1, w2, w3};
    return (__m128i)vld1q_u32(data);
}

int test_simple_transpose() {
    printf("\n=== Test 1: Simple 4x4 Transpose (32-bit elements) ===\n");

    // Input data - using simple pattern for easy verification
    // x0: [A0, A1, A2, A3]
    // x1: [B0, B1, B2, B3]
    // x2: [C0, C1, C2, C3]
    // x3: [D0, D1, D2, D3]

    uint8_t input0[16], input1[16], input2[16], input3[16];
    uint8_t expected0[16], expected1[16], expected2[16], expected3[16];
    uint8_t output0[16], output1[16], output2[16], output3[16];

    // Create input data (using simple repeating pattern)
    __m128i x0 = make_from_u32(0x00000000, 0x11111111, 0x22222222, 0x33333333);
    __m128i x1 = make_from_u32(0x44444444, 0x55555555, 0x66666666, 0x77777777);
    __m128i x2 = make_from_u32(0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB);
    __m128i x3 = make_from_u32(0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF);

    vst1q_u8(input0, (uint8x16_t)x0);
    vst1q_u8(input1, (uint8x16_t)x1);
    vst1q_u8(input2, (uint8x16_t)x2);
    vst1q_u8(input3, (uint8x16_t)x3);

    printf("\n📋 Input (before transpose):\n");
    print_as_u32("x0", input0);
    print_as_u32("x1", input1);
    print_as_u32("x2", input2);
    print_as_u32("x3", input3);

    // Compute expected result using C intrinsics
    __m128i e0 = x0, e1 = x1, e2 = x2, e3 = x3;
    __m128i t1, t2;
    transpose_4x4_c(e0, e1, e2, e3, t1, t2);

    vst1q_u8(expected0, (uint8x16_t)e0);
    vst1q_u8(expected1, (uint8x16_t)e1);
    vst1q_u8(expected2, (uint8x16_t)e2);
    vst1q_u8(expected3, (uint8x16_t)e3);

    printf("\n✓ C intrinsics computed reference result:\n");
    print_as_u32("e0", expected0);
    print_as_u32("e1", expected1);
    print_as_u32("e2", expected2);
    print_as_u32("e3", expected3);

    // Compute result using assembly
    void *input_ptrs[4] = {input0, input1, input2, input3};
    void *output_ptrs[4] = {output0, output1, output2, output3};

    test_transpose_4x4_asm(output_ptrs, input_ptrs);

    printf("\n✓ Assembly macro computed result:\n");
    print_as_u32("o0", output0);
    print_as_u32("o1", output1);
    print_as_u32("o2", output2);
    print_as_u32("o3", output3);

    // Compare results
    int errors = 0;
    if (memcmp(output0, expected0, 16) != 0) {
        printf("❌ Mismatch in output[0]\n");
        errors++;
    }
    if (memcmp(output1, expected1, 16) != 0) {
        printf("❌ Mismatch in output[1]\n");
        errors++;
    }
    if (memcmp(output2, expected2, 16) != 0) {
        printf("❌ Mismatch in output[2]\n");
        errors++;
    }
    if (memcmp(output3, expected3, 16) != 0) {
        printf("❌ Mismatch in output[3]\n");
        errors++;
    }

    if (errors == 0) {
        printf("\n✅ Test 1 passed!\n");
    } else {
        printf("\n❌ Test 1 failed with %d error(s)\n", errors);
    }

    return errors;
}

int test_sequential_transpose() {
    printf("\n=== Test 2: Sequential Pattern Transpose ===\n");

    // Input: Sequential bytes 0x00-0x3F across 4 vectors
    uint8_t input0[16], input1[16], input2[16], input3[16];
    uint8_t expected0[16], expected1[16], expected2[16], expected3[16];
    uint8_t output0[16], output1[16], output2[16], output3[16];

    // Initialize with sequential data
    for (int i = 0; i < 16; i++) {
        input0[i] = 0x00 + i;
        input1[i] = 0x10 + i;
        input2[i] = 0x20 + i;
        input3[i] = 0x30 + i;
    }

    printf("\n📋 Input:\n");
    print_hex("x0", input0, 16);
    print_hex("x1", input1, 16);
    print_hex("x2", input2, 16);
    print_hex("x3", input3, 16);

    // Compute expected result using C intrinsics
    __m128i e0 = vld1q_u64((uint64_t *)input0);
    __m128i e1 = vld1q_u64((uint64_t *)input1);
    __m128i e2 = vld1q_u64((uint64_t *)input2);
    __m128i e3 = vld1q_u64((uint64_t *)input3);
    __m128i t1, t2;

    transpose_4x4_c(e0, e1, e2, e3, t1, t2);

    vst1q_u8(expected0, (uint8x16_t)e0);
    vst1q_u8(expected1, (uint8x16_t)e1);
    vst1q_u8(expected2, (uint8x16_t)e2);
    vst1q_u8(expected3, (uint8x16_t)e3);

    printf("\n✓ Expected output:\n");
    print_hex("e0", expected0, 16);
    print_hex("e1", expected1, 16);
    print_hex("e2", expected2, 16);
    print_hex("e3", expected3, 16);

    // Compute result using assembly
    void *input_ptrs[4] = {input0, input1, input2, input3};
    void *output_ptrs[4] = {output0, output1, output2, output3};

    test_transpose_4x4_asm(output_ptrs, input_ptrs);

    printf("\n✓ Assembly output:\n");
    print_hex("o0", output0, 16);
    print_hex("o1", output1, 16);
    print_hex("o2", output2, 16);
    print_hex("o3", output3, 16);

    // Compare results
    int errors = 0;
    if (memcmp(output0, expected0, 16) != 0) {
        printf("❌ Mismatch in output[0]\n");
        errors++;
    }
    if (memcmp(output1, expected1, 16) != 0) {
        printf("❌ Mismatch in output[1]\n");
        errors++;
    }
    if (memcmp(output2, expected2, 16) != 0) {
        printf("❌ Mismatch in output[2]\n");
        errors++;
    }
    if (memcmp(output3, expected3, 16) != 0) {
        printf("❌ Mismatch in output[3]\n");
        errors++;
    }

    if (errors == 0) {
        printf("\n✅ Test 2 passed!\n");
    } else {
        printf("\n❌ Test 2 failed with %d error(s)\n", errors);
    }

    return errors;
}

int main() {
    printf("========================================\n");
    printf("transpose_4x4 Assembly Macro Test\n");
    printf("========================================\n");

    int total_errors = 0;

    total_errors += test_simple_transpose();
    total_errors += test_sequential_transpose();

    printf("\n========================================\n");
    if (total_errors == 0) {
        printf("✅ All tests PASSED!\n");
        printf("transpose_4x4 assembly macro works correctly!\n");
    } else {
        printf("❌ %d test(s) FAILED\n", total_errors);
    }
    printf("========================================\n");

    return total_errors;
}
