/*
 * Test inpack only - dump bytesliced state
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern void test_inpack_dump_asm(uint8_t *ab_out, uint8_t *cd_out, 
                                  const uint8_t *input, uint64_t key);

int main(void) {
    uint8_t input[256];
    uint8_t ab[128], cd[128];
    uint64_t key = 0x0123456789abcdefULL;

    printf("Test inpack (load + prewhiten + byteslice)\n");
    printf("==========================================\n\n");

    // Fill input with pattern where each block has unique first byte
    for (int block = 0; block < 16; block++) {
        input[block * 16] = block;  // Block ID
        for (int i = 1; i < 16; i++) {
            input[block * 16 + i] = i;
        }
    }

    printf("Input block pattern: block[0]=block_id, block[1..15]=1..15\n\n");

    test_inpack_dump_asm(ab, cd, input, key);

    printf("AB vectors (8 vectors, 16 bytes each):\n");
    for (int v = 0; v < 8; v++) {
        printf("  AB[%d]: ", v);
        for (int i = 0; i < 16; i++) {
            printf("%02x", ab[v * 16 + i]);
        }
        printf("\n");
    }

    printf("\nCD vectors (8 vectors, 16 bytes each):\n");
    for (int v = 0; v < 8; v++) {
        printf("  CD[%d]: ", v);
        for (int i = 0; i < 16; i++) {
            printf("%02x", cd[v * 16 + i]);
        }
        printf("\n");
    }

    return 0;
}
