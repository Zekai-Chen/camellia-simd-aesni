/*
 * Test C wrapper with all blocks identical
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void byteslice_16x16b_wrapper(uint8_t *blocks);

int main() {
    uint8_t blocks[256];

    // Initialize with all blocks identical: ae71c3d55ba6bf1d2c0e684aa486e0c2
    const uint8_t pattern[16] = {
        0xae, 0x71, 0xc3, 0xd5, 0x5b, 0xa6, 0xbf, 0x1d,
        0x2c, 0x0e, 0x68, 0x4a, 0xa4, 0x86, 0xe0, 0xc2
    };

    for (int i = 0; i < 16; i++) {
        memcpy(&blocks[i * 16], pattern, 16);
    }

    printf("Before byteslice (all blocks identical):\n");
    printf("  Block 0: ");
    for (int j = 0; j < 16; j++) printf("%02x", blocks[j]);
    printf("\n");

    // Call byteslice
    byteslice_16x16b_wrapper(blocks);

    printf("\nAfter byteslice:\n");
    printf("  Vector 0: ");
    for (int j = 0; j < 16; j++) printf("%02x", blocks[j]);
    printf("\n");
    printf("  Expected: aeaeaeaeaeaeaeaeaeaeaeaeaeaeaeae\n");

    printf("  Vector 1: ");
    for (int j = 0; j < 16; j++) printf("%02x", blocks[16 + j]);
    printf("\n");
    printf("  Expected: 71717171717171717171717171717171\n");

    // Check if correct
    int match = 1;
    for (int i = 0; i < 16; i++) {
        if (blocks[i] != 0xae) match = 0;
        if (blocks[16 + i] != 0x71) match = 0;
    }

    if (match) {
        printf("\nSUCCESS!\n");
    } else {
        printf("\nFAILURE: Output doesn't match expected\n");
    }

    return match ? 0 : 1;
}
