/*
 * Trace byteslice transformation phase by phase
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Simple test input: sequential bytes
static uint8_t test_input[256];

// Initialize with sequential pattern
void init_test_input() {
    for (int i = 0; i < 256; i++) {
        test_input[i] = i;
    }
}

// C reference - manually inline to add phase tracing
void c_byteslice_trace(uint8_t output_ab[128], uint8_t output_cd[128]) {
    uint8x16_t a0, a1, a2, a3, b0, b1, b2, b3;
    uint8x16_t c0, c1, c2, c3, d0, d1, d2, d3;
    
    // Load input
    a0 = vld1q_u8(&test_input[0]);
    a1 = vld1q_u8(&test_input[16]);
    a2 = vld1q_u8(&test_input[32]);
    a3 = vld1q_u8(&test_input[48]);
    b0 = vld1q_u8(&test_input[64]);
    b1 = vld1q_u8(&test_input[80]);
    b2 = vld1q_u8(&test_input[96]);
    b3 = vld1q_u8(&test_input[112]);
    c0 = vld1q_u8(&test_input[128]);
    c1 = vld1q_u8(&test_input[144]);
    c2 = vld1q_u8(&test_input[160]);
    c3 = vld1q_u8(&test_input[176]);
    d0 = vld1q_u8(&test_input[192]);
    d1 = vld1q_u8(&test_input[208]);
    d2 = vld1q_u8(&test_input[224]);
    d3 = vld1q_u8(&test_input[240]);
    
    printf("C Phase 0 (input):\n");
    printf("  c0[0-3]: %02x %02x %02x %02x\n", 
           vgetq_lane_u8(c0, 0), vgetq_lane_u8(c0, 1), 
           vgetq_lane_u8(c0, 2), vgetq_lane_u8(c0, 3));
    printf("  d2[0-3]: %02x %02x %02x %02x\n",
           vgetq_lane_u8(d2, 0), vgetq_lane_u8(d2, 1),
           vgetq_lane_u8(d2, 2), vgetq_lane_u8(d2, 3));
    
    // Phase 1: 4x4 transposes (on 32-bit elements)
    // Simulate transpose_4x4 for groups
    // ... (simplified, just show key vectors)
    
    // Store final output
    vst1q_u8(&output_cd[0], c0);
    vst1q_u8(&output_cd[16], c1);
    vst1q_u8(&output_cd[32], c2);
    vst1q_u8(&output_cd[48], c3);
    vst1q_u8(&output_cd[64], d0);
    vst1q_u8(&output_cd[80], d1);
    vst1q_u8(&output_cd[96], d2);
    vst1q_u8(&output_cd[112], d3);
}

// Assembly with phase tracing
extern void asm_byteslice_trace(uint8_t *input, uint8_t *output_cd);

int main() {
    uint8_t ab_c[128], cd_c[128];
    uint8_t cd_asm[128];
    
    init_test_input();
    
    printf("========================================\n");
    printf("Byteslice Phase-by-Phase Trace\n");
    printf("========================================\n\n");
    
    printf("Input blocks (first 4 bytes of each):\n");
    for (int i = 0; i < 16; i++) {
        printf("  Block %2d: %02x %02x %02x %02x\n", i,
               test_input[i*16+0], test_input[i*16+1],
               test_input[i*16+2], test_input[i*16+3]);
    }
    printf("\n");
    
    // Run C version
    c_byteslice_trace(ab_c, cd_c);
    
    printf("\nC Final CD output:\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 4; j++) {
            printf("%02x ", cd_c[i*16+j]);
        }
        printf("\n");
    }
    
    printf("\n");
    
    // Run assembly version (with debug prints inside)
    asm_byteslice_trace(test_input, cd_asm);
    
    printf("\nASM Final CD output:\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 4; j++) {
            printf("%02x ", cd_asm[i*16+j]);
        }
        printf("\n");
    }
    
    return 0;
}
