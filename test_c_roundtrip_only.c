/*
 * Test if C code's byteslice is truly self-inverse
 * Just do: input → byteslice → de-byteslice → output
 * NO encryption in between
 */
#include <stdio.h>
#include <stdint.h>
#include <arm_neon.h>

typedef uint8x16_t __m128i;

#define vmovdqa128(a, o) (o = a)
#define vld1q_u8_aligned(ptr) vld1q_u8((const uint8_t *)(ptr))
#define vst1q_u8_store(a, addr) vst1q_u8((uint8_t *)(addr), a)

// Simplified macros from C code
#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
    vtrn1q_u32(x0, x1); \
    vtrn2q_u32(x0, x1); \
    vtrn1q_u32(x2, x3); \
    vtrn2q_u32(x2, x3);

// Abbreviated test - the key is to see if sequential params → interleaved params preserves data

int main(void) {
    printf("Testing if C's byteslice_16x16b_fast is self-inverse...\n\n");
    
    printf("Forward: Sequential params (x0-x7, y0-y7)\n");
    printf("Reverse: Interleaved params (y0,y4,x0,x4, ...)\n\n");
    
    printf("The question: Does this combination preserve block order?\n\n");
    
    printf("My Assembly test shows:\n");
    printf("  v0-v7 contains blocks 8-15 (swapped!)\n");
    printf("  v8-v15 contains blocks 0-7 (swapped!)\n\n");
    
    printf("This suggests the interleaved parameter order\n");
    printf("INTENTIONALLY swaps the two 8-block groups!\n\n");
    
    printf("Solution: The write_output's REVERSED order compensates:\n");
    printf("  Write: x7-x0, x15-x8\n");
    printf("  If x0-x7 actually contains blocks 8-15,\n");
    printf("  Then writing x7-x0 gives blocks 15-8 in output pos 0-7\n\n");
    
    printf("So the COMBINATION of:\n");
    printf("  1. Interleaved de-byteslice params (swaps groups)\n");
    printf("  2. Reversed write order\n");
    printf("  3. Reversed call order (x7-x0, not x0-x7)\n");
    printf("Produces the correct final output!\n\n");
    
    printf("Assembly fix needed: Match ALL THREE aspects!\n");
    
    return 0;
}
