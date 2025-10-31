/*
 * Compare first roundsm16 between C and Assembly
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern uint8_t debug_ab_before_first_round[128];
extern uint8_t debug_cd_after_byteslice[128];
extern uint8_t debug_cd_after_first_round[128];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

int main(void) {
    struct camellia_simd_ctx ctx_c, ctx_asm;
    uint8_t input[256], output_c[256], output_asm[256];

    // Initialize: all 16 blocks with same plaintext
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("========================================\n");
    printf("First Roundsm16 Comparison\n");
    printf("========================================\n\n");

    // Test C reference
    printf("=== C REFERENCE ===\n");
    camellia_keysetup_simd128(&ctx_c, test_key, 16);
    camellia_encrypt_16blks_simd128(&ctx_c, output_c, input);

    // Test Assembly
    printf("\n=== ASSEMBLY ===\n");
    camellia_keysetup_simd128(&ctx_asm, test_key, 16);
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx_asm, output_asm, input);

    printf("\n--- Assembly: AB after inpack byteslice ---\n");
    for (int i = 0; i < 8; i++) {
        printf("  AB[%d]: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", debug_ab_before_first_round[i * 16 + j]);
        printf("\n");
    }

    printf("\n--- Assembly: CD after inpack byteslice ---\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", debug_cd_after_byteslice[i * 16 + j]);
        printf("\n");
    }

    printf("\n--- Assembly: CD after first roundsm16 ---\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", debug_cd_after_first_round[i * 16 + j]);
        printf("\n");
    }

    // Compare final outputs
    printf("\n--- Final outputs ---\n");
    printf("C output block 0:   ");
    for (int i = 0; i < 16; i++) printf("%02x", output_c[i]);
    printf("\n");

    printf("ASM output block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_asm[i]);
    printf("\n");

    int match = (memcmp(output_c, output_asm, 256) == 0);
    printf("\n%s\n", match ? "*** SUCCESS ***" : "*** FAILURE ***");

    return match ? 0 : 1;
}
