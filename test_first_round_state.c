/*
 * Test to capture and compare state after first round
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

// Test vectors
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// Assembly function that captures state after first round
extern void test_capture_first_round_asm(uint8_t *ab_out, uint8_t *cd_out,
                                          const uint8_t *input,
                                          struct camellia_simd_ctx *ctx);

// For now, just use C encrypt and manually capture
// (This is a placeholder - we'll compare Assembly output manually)
void test_capture_first_round_c(uint8_t *ab_out, uint8_t *cd_out,
                                 const uint8_t *input,
                                 struct camellia_simd_ctx *ctx) {
    // TODO: Implement proper C version if needed
    // For now, just zero out to avoid comparison
    memset(ab_out, 0, 128);
    memset(cd_out, 0, 128);
}

void print_state(const char *label, const uint8_t *data, int num_vectors) {
    printf("%s:\n", label);
    for (int i = 0; i < num_vectors && i < 4; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", data[i * 16 + j]);
        }
        printf("\n");
    }
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256];
    uint8_t ab_c[128], cd_c[128];
    uint8_t ab_asm[128], cd_asm[128];

    printf("========================================\n");
    printf("First Round State Comparison\n");
    printf("========================================\n\n");

    // Setup key
    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // Prepare input (all blocks identical)
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_plaintext, 16);
    }

    printf("Step 1: Capture state after first round (C implementation)\n");
    test_capture_first_round_c(ab_c, cd_c, input, &ctx);
    print_state("  AB (C)", ab_c, 4);
    printf("\n");
    print_state("  CD (C)", cd_c, 4);
    printf("\n");

    printf("Step 2: Capture state after first round (Assembly)\n");
    test_capture_first_round_asm(ab_asm, cd_asm, input, &ctx);
    print_state("  AB (ASM)", ab_asm, 4);
    printf("\n");
    print_state("  CD (ASM)", cd_asm, 4);
    printf("\n");

    // Compare AB state
    printf("Step 3: Compare AB state\n");
    int ab_errors = 0;
    for (int i = 0; i < 128; i++) {
        if (ab_c[i] != ab_asm[i]) {
            if (ab_errors < 16) {
                printf("  AB[%3d]: C=%02x ASM=%02x (diff=%02x)\n",
                       i, ab_c[i], ab_asm[i], ab_c[i] ^ ab_asm[i]);
            }
            ab_errors++;
        }
    }
    printf("  AB: %d/128 bytes match\n", 128 - ab_errors);

    // Compare CD state
    printf("\nStep 4: Compare CD state\n");
    int cd_errors = 0;
    for (int i = 0; i < 128; i++) {
        if (cd_c[i] != cd_asm[i]) {
            if (cd_errors < 16) {
                printf("  CD[%3d]: C=%02x ASM=%02x (diff=%02x)\n",
                       i, cd_c[i], cd_asm[i], cd_c[i] ^ cd_asm[i]);
            }
            cd_errors++;
        }
    }
    printf("  CD: %d/128 bytes match\n", 128 - cd_errors);

    printf("\n");
    if (ab_errors == 0 && cd_errors == 0) {
        printf("✓ First round state matches!\n");
        return 0;
    } else {
        printf("✗ First round state differs (%d AB errors, %d CD errors)\n",
               ab_errors, cd_errors);
        return 1;
    }
}
