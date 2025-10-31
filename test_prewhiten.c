/*
 * Test to understand pre-whitening in C implementation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Test key
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    // Load the key as a 64-bit value (what C code does)
    uint64_t key_u64;
    memcpy(&key_u64, test_key_128, 8);

    printf("Key bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", test_key_128[i]);
    }
    printf("\n");

    printf("Key as uint64_t (first 8 bytes): 0x%016lx\n", key_u64);

    // Duplicate to 128-bit
    uint8x16_t key_vec = vdupq_n_u8(0);
    memcpy(&key_vec, &key_u64, 8);

    printf("Key vector (as uint64x2): [%016lx, %016lx]\n",
           vgetq_lane_u64(vreinterpretq_u64_u8(key_vec), 0),
           vgetq_lane_u64(vreinterpretq_u64_u8(key_vec), 1));

    // Now see what a dup does
    uint64x2_t key_dup = vdupq_n_u64(key_u64);
    printf("After vdupq_n_u64: [%016lx, %016lx]\n",
           vgetq_lane_u64(key_dup, 0),
           vgetq_lane_u64(key_dup, 1));

    // Print as bytes
    uint8_t result[16];
    vst1q_u8(result, vreinterpretq_u8_u64(key_dup));
    printf("As bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", result[i]);
    }
    printf("\n");

    return 0;
}
