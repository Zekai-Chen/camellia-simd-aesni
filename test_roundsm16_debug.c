/*
 * Debug test for roundsm16 - compare inputs and outputs
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
extern uint8_t debug_ab_on_stack_after_store[128];
extern uint8_t debug_stack_memory_before_load[128];
extern uint8_t debug_ab_loaded_from_stack[128];
extern uint8_t debug_roundsm16_phase0_ab[128];
extern uint8_t debug_roundsm16_phase1_ab[128];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

void print_vec(const char *label, const uint8_t *data, int idx) {
    printf("  %s[%d]: ", label, idx);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[idx * 16 + i]);
    }

    // Check if all bytes are identical
    int all_same = 1;
    for (int i = 1; i < 16; i++) {
        if (data[idx * 16 + i] != data[idx * 16]) {
            all_same = 0;
            break;
        }
    }
    if (all_same) {
        printf(" ✓ (all bytes = %02x)", data[idx * 16]);
    } else {
        printf(" ✗ (mixed bytes)");
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];

    // Initialize: all 16 blocks with same plaintext
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("========================================\n");
    printf("roundsm16 Debug Test\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key, 16);
    printf("Key for first round (k+2): 0x%016lx\n\n", ctx.key_table[2]);

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);

    printf("--- Input to first roundsm16 (AB after byteslice) ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("AB", debug_ab_before_first_round, i);
    }

    printf("\n--- Input to first roundsm16 (CD after byteslice) ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("CD", debug_cd_after_byteslice, i);
    }

    printf("\n--- AB on stack immediately after store ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("AB", debug_ab_on_stack_after_store, i);
    }

    printf("\n--- Stack memory at [sp,#0] before load (scalar ldp) ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("MEM", debug_stack_memory_before_load, i);
    }

    printf("\n--- AB loaded from stack (v0-v7 after ldp q) ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("AB", debug_ab_loaded_from_stack, i);
    }

    printf("\n--- Phase 0: AB BEFORE inv_shift_rows (inside roundsm16) ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("AB", debug_roundsm16_phase0_ab, i);
    }

    printf("\n--- Phase 1: AB AFTER inv_shift_rows ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("AB", debug_roundsm16_phase1_ab, i);
    }

    printf("\n--- Output from first roundsm16 (new CD) ---\n");
    for (int i = 0; i < 8; i++) {
        print_vec("CD", debug_cd_after_first_round, i);
    }

    printf("\n========================================\n");
    printf("Expected from C: All output vectors should have\n");
    printf("identical bytes within each vector.\n");
    printf("If Phase 1 output is uniform, inv_shift_rows is OK.\n");
    printf("========================================\n");

    return 0;
}
