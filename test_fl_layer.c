/*
 * Test FL layer in isolation
 * Capture AB/CD state before and after FL transformation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// External assembly function to capture FL state
extern void test_fl_capture_asm(struct camellia_simd_ctx *ctx,
                                 void *ab_before, void *cd_before,
                                 void *ab_after, void *cd_after);

void hex_dump_block(const char *label, const uint8_t *data, int len) {
    printf("%s:\n", label);
    for (int i = 0; i < len; i += 16) {
        printf("  %04x:", i);
        for (int j = 0; j < 16 && i + j < len; j++) {
            if (j % 4 == 0) printf(" ");
            printf("%02x", data[i + j]);
        }
        printf("\n");
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t ab_before[128], cd_before[128];
    uint8_t ab_after[128], cd_after[128];

    printf("========================================\n");
    printf("FL Layer Isolated Test\n");
    printf("========================================\n\n");

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    printf("Key table[8] (FL key):    %016lx\n", ctx.key_table[8]);
    printf("Key table[9] (FLINV key): %016lx\n\n", ctx.key_table[9]);

    // Call assembly test function
    test_fl_capture_asm(&ctx, ab_before, cd_before, ab_after, cd_after);

    printf("AB state BEFORE FL (first 64 bytes):\n");
    hex_dump_block("", ab_before, 64);

    printf("\nCD state BEFORE FL (first 64 bytes):\n");
    hex_dump_block("", cd_before, 64);

    printf("\nAB state AFTER FL (first 64 bytes):\n");
    hex_dump_block("", ab_after, 64);

    printf("\nCD state AFTER FL (first 64 bytes):\n");
    hex_dump_block("", cd_after, 64);

    // Check if FL changed anything
    int ab_changed = memcmp(ab_before, ab_after, 128);
    int cd_changed = memcmp(cd_before, cd_after, 128);

    printf("\nFL transformation results:\n");
    printf("  AB changed: %s\n", ab_changed ? "YES" : "NO");
    printf("  CD changed: %s\n", cd_changed ? "YES" : "NO");

    return 0;
}
