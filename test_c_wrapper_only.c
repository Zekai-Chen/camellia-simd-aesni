/*
 * Test C byteslice wrapper in isolation
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void byteslice_16x16b_wrapper(uint8_t *blocks);

void print_block(const char *name, uint8_t *block) {
    printf("%s: ", name);
    for (int i = 0; i < 16; i++) {
        printf("%02x", block[i]);
    }
    printf("\n");
}

int main() {
    uint8_t blocks[256];

    // Initialize with simple pattern: block i has all bytes = i
    for (int i = 0; i < 16; i++) {
        memset(&blocks[i * 16], i, 16);
    }

    printf("Before byteslice:\n");
    for (int i = 0; i < 16; i++) {
        printf("  Block %2d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", blocks[i * 16 + j]);
        }
        printf("\n");
    }

    // Call byteslice
    byteslice_16x16b_wrapper(blocks);

    printf("\nAfter byteslice:\n");
    for (int i = 0; i < 16; i++) {
        printf("  Vector %2d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", blocks[i * 16 + j]);
        }
        printf("\n");
    }

    return 0;
}
