/*
 * Detailed debug test with FL/FLINV tracking
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
extern uint8_t debug_ab_before_fl[128];
extern uint8_t debug_ab_after_fl[128];
extern uint8_t debug_cd_before_fl[128];
extern uint8_t debug_cd_after_fl[128];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

void print_vec(const char *label, const uint8_t *data) {
    printf("  %s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx_c, ctx_asm;
    uint8_t input[256], output_c[256], output_asm[256];

    printf("========================================\n");
    printf("Detailed Debug Test\n");
    printf("========================================\n\n");

    // Initialize: all 16 blocks with same plaintext
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    // Test C reference
    printf("=== C REFERENCE ===\n");
    camellia_keysetup_simd128(&ctx_c, test_key, 16);
    camellia_encrypt_16blks_simd128(&ctx_c, output_c, input);
    printf("C output block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_c[i]);
    printf("\n");

    // Test Assembly
    printf("\n=== ASSEMBLY ===\n");
    camellia_keysetup_simd128(&ctx_asm, test_key, 16);
    
    // Clear debug variables
    memset(debug_ab_before_fl, 0, 128);
    memset(debug_ab_after_fl, 0, 128);
    memset(debug_cd_before_fl, 0, 128);
    memset(debug_cd_after_fl, 0, 128);
    
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx_asm, output_asm, input);

    printf("\n--- After inpack byteslice ---\n");
    printf("AB vectors:\n");
    for (int i = 0; i < 8; i++) {
        printf("  AB[%d]: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", debug_ab_before_first_round[i * 16 + j]);
        printf("\n");
    }
    printf("CD vectors:\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 16; j++) printf("%02x", debug_cd_after_byteslice[i * 16 + j]);
        printf("\n");
    }

    // Check if FL/FLINV was called (non-zero debug data)
    int fl_called = 0;
    for (int i = 0; i < 16; i++) {
        if (debug_ab_before_fl[i] != 0) {
            fl_called = 1;
            break;
        }
    }

    if (fl_called) {
        printf("\n--- Before FL/FLINV (k=6) ---\n");
        printf("AB vectors:\n");
        for (int i = 0; i < 8; i++) {
            printf("  AB[%d]: ", i);
            for (int j = 0; j < 16; j++) printf("%02x", debug_ab_before_fl[i * 16 + j]);
            printf("\n");
        }
        printf("CD vectors:\n");
        for (int i = 0; i < 8; i++) {
            printf("  CD[%d]: ", i);
            for (int j = 0; j < 16; j++) printf("%02x", debug_cd_before_fl[i * 16 + j]);
            printf("\n");
        }

        printf("\n--- After FL/FLINV (k=6) ---\n");
        printf("AB vectors:\n");
        for (int i = 0; i < 8; i++) {
            printf("  AB[%d]: ", i);
            for (int j = 0; j < 16; j++) printf("%02x", debug_ab_after_fl[i * 16 + j]);
            printf("\n");
        }
        printf("CD vectors:\n");
        for (int i = 0; i < 8; i++) {
            printf("  CD[%d]: ", i);
            for (int j = 0; j < 16; j++) printf("%02x", debug_cd_after_fl[i * 16 + j]);
            printf("\n");
        }
    } else {
        printf("\nWARNING: FL/FLINV debug data is all zeros - was FL/FLINV called?\n");
    }

    printf("\n--- Final output ---\n");
    printf("Assembly output block 0: ");
    for (int i = 0; i < 16; i++) printf("%02x", output_asm[i]);
    printf("\n");

    // Compare
    int match = 1;
    for (int i = 0; i < 256; i++) {
        if (output_c[i] != output_asm[i]) {
            match = 0;
            break;
        }
    }

    if (match) {
        printf("\n*** SUCCESS: All blocks match! ***\n");
        return 0;
    } else {
        printf("\n*** FAILURE: Output differs ***\n");
        printf("C:   %s\n", "67673138549669730857065648eabe43");
        printf("ASM: ");
        for (int i = 0; i < 16; i++) printf("%02x", output_asm[i]);
        printf("\n");
        return 1;
    }
}
