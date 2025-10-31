/*
 * test_asm_filter_8bit.c - Test the filter_8bit macro in assembly
 *
 * This test verifies that the filter_8bit macro implemented in
 * camellia_simd128_aarch64_neon_crypto.S works correctly by comparing
 * its output against the known-good C intrinsics version.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arm_neon.h>

// Test function that we'll implement in assembly
// This function applies filter_8bit to a single vector
extern void test_filter_8bit_asm(
    uint8_t *output,        // 16 bytes output
    const uint8_t *input,   // 16 bytes input
    const uint8_t *lo_t,    // 16 bytes low table
    const uint8_t *hi_t     // 16 bytes high table
);

// Reference C implementation using intrinsics (from test_03)
void filter_8bit_c(uint8x16_t *x, uint8x16_t lo_t, uint8x16_t hi_t, uint8x16_t mask4bit) {
    uint8x16_t tmp0;

    // Extract low nibbles
    tmp0 = vandq_u8(*x, mask4bit);

    // Extract high nibbles
    *x = vshrq_n_u8(*x, 4);

    // Table lookups
    tmp0 = vqtbl1q_u8(lo_t, tmp0);
    *x = vqtbl1q_u8(hi_t, *x);

    // XOR combine
    *x = veorq_u8(tmp0, *x);
}

void print_hex(const char *name, const uint8_t *data, int len) {
    printf("%s: ", name);
    for (int i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if (i % 4 == 3 && i != len - 1) printf(" ");
    }
    printf("\n");
}

int main() {
    printf("========================================\n");
    printf("filter_8bit Assembly Macro Test\n");
    printf("========================================\n\n");

    // Test tables (simple identity-like transform)
    uint8_t lo_table[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };

    uint8_t hi_table[16] = {
        0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
        0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0
    };

    // Test input
    uint8_t input[16] = {
        0x00, 0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
        0x78, 0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef
    };

    printf("📋 Test Setup:\n");
    print_hex("Input    ", input, 16);
    print_hex("Lo Table ", lo_table, 16);
    print_hex("Hi Table ", hi_table, 16);
    printf("\n");

    // Compute expected result using C intrinsics
    uint8_t expected[16];
    memcpy(expected, input, 16);
    uint8x16_t x_c = vld1q_u8(expected);
    uint8x16_t lo_t = vld1q_u8(lo_table);
    uint8x16_t hi_t = vld1q_u8(hi_table);
    uint8x16_t mask = vdupq_n_u8(0x0f);

    filter_8bit_c(&x_c, lo_t, hi_t, mask);
    vst1q_u8(expected, x_c);

    printf("✓ C intrinsics computed reference result\n");
    print_hex("Expected ", expected, 16);
    printf("\n");

    // Compute result using assembly
    uint8_t output[16] = {0};
    test_filter_8bit_asm(output, input, lo_table, hi_table);

    printf("✓ Assembly macro computed result\n");
    print_hex("Got      ", output, 16);
    printf("\n");

    // Compare results
    int errors = 0;
    for (int i = 0; i < 16; i++) {
        if (output[i] != expected[i]) {
            printf("❌ Mismatch at byte %d: got 0x%02x, expected 0x%02x\n",
                   i, output[i], expected[i]);
            errors++;
        }
    }

    printf("========================================\n");
    if (errors == 0) {
        printf("✅ filter_8bit assembly macro works correctly!\n");
    } else {
        printf("❌ %d error(s) found\n", errors);
    }
    printf("========================================\n");

    return errors;
}
