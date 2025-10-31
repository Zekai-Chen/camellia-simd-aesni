/*
 * Test transpose_4x4 only - verify assembly matches C
 */
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>

// C transpose (from camellia header)
#define vpunpckhdq128(a, b, o)  (o = (__m128i)vzip2q_u32((uint32x4_t)(b), (uint32x4_t)(a)))
#define vpunpckldq128(a, b, o)  (o = (__m128i)vzip1q_u32((uint32x4_t)(b), (uint32x4_t)(a)))
#define vpunpckhqdq128(a, b, o) (o = (__m128i)vzip2q_u64((uint64x2_t)(b), (uint64x2_t)(a)))
#define vpunpcklqdq128(a, b, o) (o = (__m128i)vzip1q_u64((uint64x2_t)(b), (uint64x2_t)(a)))

#define transpose_4x4_c(x0, x1, x2, x3, t1, t2) \
    vpunpckhdq128(x1, x0, t2); \
    vpunpckldq128(x1, x0, x0); \
    vpunpckldq128(x3, x2, t1); \
    vpunpckhdq128(x3, x2, x2); \
    vpunpckhqdq128(t1, x0, x1); \
    vpunpcklqdq128(t1, x0, x0); \
    vpunpckhqdq128(x2, t2, x3); \
    vpunpcklqdq128(x2, t2, x2);

typedef uint8x16_t __m128i;

extern void transpose_4x4_asm(uint8_t out[64], const uint8_t in[64]);

void print_vec(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) printf("%02x", data[i]);
    printf("\n");
}

int main() {
    // Test input: 4 vectors with distinct patterns
    uint8_t in[64] = {
        // x0: 00 01 02 03 ...
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        // x1: 10 11 12 13 ...
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        // x2: 20 21 22 23 ...
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        // x3: 30 31 32 33 ...
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
    };

    uint8_t out_c[64], out_asm[64];

    // C transpose
    __m128i x0 = vld1q_u8(&in[0]);
    __m128i x1 = vld1q_u8(&in[16]);
    __m128i x2 = vld1q_u8(&in[32]);
    __m128i x3 = vld1q_u8(&in[48]);
    __m128i t1, t2;

    transpose_4x4_c(x0, x1, x2, x3, t1, t2);

    vst1q_u8(&out_c[0], x0);
    vst1q_u8(&out_c[16], x1);
    vst1q_u8(&out_c[32], x2);
    vst1q_u8(&out_c[48], x3);

    // Assembly transpose
    transpose_4x4_asm(out_asm, in);

    printf("Transpose 4x4 Test\n");
    printf("==================\n\n");

    printf("C output:\n");
    print_vec("  x0", &out_c[0]);
    print_vec("  x1", &out_c[16]);
    print_vec("  x2", &out_c[32]);
    print_vec("  x3", &out_c[48]);

    printf("\nAssembly output:\n");
    print_vec("  x0", &out_asm[0]);
    print_vec("  x1", &out_asm[16]);
    print_vec("  x2", &out_asm[32]);
    print_vec("  x3", &out_asm[48]);

    // Compare
    int match = 1;
    for (int i = 0; i < 64; i++) {
        if (out_c[i] != out_asm[i]) {
            match = 0;
            break;
        }
    }

    printf("\nResult: %s\n", match ? "MATCH ✓" : "MISMATCH ✗");
    return match ? 0 : 1;
}
