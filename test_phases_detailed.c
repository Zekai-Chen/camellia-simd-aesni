/*
 * Detailed phase-by-phase comparison
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern void test_roundsm16_phase_by_phase(struct camellia_simd_ctx *ctx,
                                           void *phase_outputs);

void print_phase(const char *name, uint8_t *data, int offset) {
    printf("%s:\n", name);
    printf("  [0]: ");
    for (int i = 0; i < 16; i++) {
        if (i > 0 && i % 4 == 0) printf(" ");
        printf("%02x", data[offset + i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t phase_outputs[1024];  // 8 phases × 128 bytes
    
    printf("========================================\n");
    printf("Roundsm16 Phase-by-Phase Analysis\n");
    printf("========================================\n\n");
    
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    
    printf("Input: Zero AB state\n");
    printf("Key table[2]: %016lx\n\n", ctx.key_table[2]);
    
    test_roundsm16_phase_by_phase(&ctx, phase_outputs);
    
    print_phase("Phase 0: Input", phase_outputs, 0);
    print_phase("Phase 1: After Inverse ShiftRows", phase_outputs, 128);
    print_phase("Phase 2: After Pre-filter", phase_outputs, 256);
    print_phase("Phase 3: After SubBytes (AESE)", phase_outputs, 384);
    print_phase("Phase 4: After Post-filter", phase_outputs, 512);
    print_phase("Phase 5: After P-function (8 XORs)", phase_outputs, 640);
    print_phase("Phase 5 complete: After P-function (16 XORs)", phase_outputs, 768);
    print_phase("Phase 6: After Key addition", phase_outputs, 896);
    
    return 0;
}
