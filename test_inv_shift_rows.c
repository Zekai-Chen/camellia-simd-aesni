/*
 * Test if inv_shift_rows mask works correctly
 */
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>

// External inv_shift_rows mask from Assembly
extern const uint8_t inv_shift_rows_mask[16];

int main(void) {
    // Create uniform input
    uint8_t input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0xAE;
    }

    // Load mask and input
    uint8x16_t mask = vld1q_u8(inv_shift_rows_mask);
    uint8x16_t data = vld1q_u8(input);

    printf("Input:  ");
    for (int i = 0; i < 16; i++) printf("%02x", input[i]);
    printf("\n");

    printf("Mask:   ");
    uint8_t mask_bytes[16];
    vst1q_u8(mask_bytes, mask);
    for (int i = 0; i < 16; i++) printf("%02x", mask_bytes[i]);
    printf("\n");

    // Apply tbl (same as Assembly does)
    uint8x16_t result = vqtbl1q_u8(data, mask);

    uint8_t output[16];
    vst1q_u8(output, result);

    printf("Output: ");
    for (int i = 0; i < 16; i++) printf("%02x", output[i]);
    printf("\n");

    // Check uniformity
    int all_same = 1;
    for (int i = 1; i < 16; i++) {
        if (output[i] != output[0]) {
            all_same = 0;
            break;
        }
    }

    if (all_same) {
        printf("✓ Output is uniform (all bytes = %02x)\n", output[0]);
        return 0;
    } else {
        printf("✗ Output is NOT uniform - BUG!\n");
        return 1;
    }
}
