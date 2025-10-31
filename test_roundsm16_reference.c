/*
 * Generate reference values for roundsm16 testing
 * This extracts intermediate values from C implementation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Copy necessary macros from C implementation
typedef uint8x16_t __m128i;

#define vmovdqa128(a, o) (o = a)
#define vpxor128(a, b, o) (o = veorq_u8(a, b))
#define vld1q_u8_aligned(ptr) vld1q_u8((const uint8_t *)(ptr))

// Simplified roundsm16 implementation for debugging
// We'll manually implement just the key parts to see what's happening

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_hex(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

static void print_vector(const char *label, const __m128i v) {
    uint8_t buf[16];
    vst1q_u8(buf, v);
    print_hex(label, buf, 16);
}

int main(void) {
    printf("========================================\n");
    printf("Roundsm16 Reference Value Generator\n");
    printf("========================================\n\n");

    // We need to call the actual C implementation to get reference values
    // But since it doesn't expose internals, let's document what we know:

    printf("Test Setup:\n");
    printf("  Key:   ");
    print_hex("", test_key_128, 16);
    printf("  Plain: ");
    print_hex("", test_plaintext, 16);
    printf("\n");

    // From key setup, we know:
    // key_table[0] = 0xd20d72f2af5286b2 (kw1)
    // key_table[1] = 0x0000000000000000 (kw2)
    // key_table[2] = 0xe3aca046e84efa3f (k1)

    uint64_t kw1 = 0xd20d72f2af5286b2ULL;
    uint64_t kw2 = 0x0000000000000000ULL;
    uint64_t k1  = 0xe3aca046e84efa3fULL;

    printf("Key schedule:\n");
    printf("  kw1 (key_table[0]) = 0x%016lx\n", kw1);
    printf("  kw2 (key_table[1]) = 0x%016lx\n", kw2);
    printf("  k1  (key_table[2]) = 0x%016lx\n\n", k1);

    // After byteslice + prewhiten with identical blocks:
    // AB vectors contain: [byte0^kw1, byte2^kw1, byte1^kw1, byte3^kw1, ...]
    // CD vectors contain: [byte8^kw2, byte10^kw2, byte9^kw2, byte11^kw2, ...]

    printf("State after byteslice + prewhiten:\n");
    printf("  (With 16 identical plaintext blocks)\n\n");

    // Calculate AB[0] (byte 0 of plaintext XOR with lowest byte of kw1)
    uint8_t ab0_val = 0x01 ^ (uint8_t)(kw1 & 0xff);
    uint8_t ab1_val = 0x45 ^ (uint8_t)(kw1 & 0xff);  // byte 2 (interleaved)
    uint8_t ab2_val = 0x23 ^ (uint8_t)(kw1 & 0xff);  // byte 1 (interleaved)
    uint8_t ab3_val = 0x67 ^ (uint8_t)(kw1 & 0xff);  // byte 3

    printf("  AB[0] should be: 0x%02x (repeated 16 times)\n", ab0_val);
    printf("  AB[1] should be: 0x%02x (repeated 16 times)\n", ab1_val);
    printf("  AB[2] should be: 0x%02x (repeated 16 times)\n", ab2_val);
    printf("  AB[3] should be: 0x%02x (repeated 16 times)\n\n", ab3_val);

    // CD is just plaintext bytes since kw2=0
    printf("  CD[0] should be: 0x89 (repeated 16 times)\n");
    printf("  CD[1] should be: 0xcd (repeated 16 times)\n");
    printf("  CD[2] should be: 0xab (repeated 16 times)\n");
    printf("  CD[3] should be: 0xef (repeated 16 times)\n\n");

    printf("Next: Need to trace through roundsm16 step by step\n");
    printf("This requires instrumenting the C code or comparing final output\n\n");

    printf("Alternative approach: Use test_first_round.c to compare\n");
    printf("C implementation vs Assembly at first round boundary\n");

    return 0;
}
