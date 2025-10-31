/*
 * Test pack_bswap shuffle mask
 */
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>

int main(void) {
    // Test value: 0xd20d72f2af5286b2
    uint64_t key = 0xd20d72f2af5286b2UL;

    // C code simulation
    uint8x16_t v_c;
    uint64x2_t tmp = {key, 0};
    v_c = vreinterpretq_u8_u64(tmp);

    // Apply pack_bswap shuffle
    uint8_t mask[16] = {3, 2, 1, 0, 7, 6, 5, 4, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f};
    uint8x16_t mask_v = vld1q_u8(mask);
    uint8x16_t result_c = vqtbl1q_u8(v_c, mask_v);

    uint8_t buf1[16], buf2[16];
    vst1q_u8(buf1, v_c);
    vst1q_u8(buf2, result_c);

    printf("Input key: 0x%016lx\n", key);
    printf("After load to lower 64 bits:\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", buf1[i]);
    }
    printf("\n");

    printf("After pack_bswap shuffle:\n  ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", buf2[i]);
    }
    printf("\n");

    printf("\nExpected (byte-swapped key in lower 64 bits):\n");
    printf("  b2 86 52 af f2 72 0d d2 00 00 00 00 00 00 00 00\n");

    return 0;
}
