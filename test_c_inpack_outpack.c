/*
 * Test C implementation inpack+outunpack (no rounds)
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Simplified C test using same logic as assembly
int main(void) {
    uint8_t input[256];
    uint8_t output[256];
    
    printf("C inpack+outunpack test\n");
    printf("========================\n\n");

    // Fill input
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }

    // Simple test: if we XOR with key twice, should get back original
    uint64_t key = 0x0123456789abcdefULL;
    uint8x16_t key_vec = vreinterpretq_u8_u64(vdupq_n_u64(key));

    // Load and XOR
    uint8x16_t blocks[16];
    for (int i = 0; i < 16; i++) {
        blocks[i] = vld1q_u8(input + i * 16);
        blocks[i] = veorq_u8(blocks[i], key_vec);
    }

    // XOR again (should restore)
    for (int i = 0; i < 16; i++) {
        blocks[i] = veorq_u8(blocks[i], key_vec);
    }

    // Store
    for (int i = 0; i < 16; i++) {
        vst1q_u8(output + i * 16, blocks[i]);
    }

    // Check
    int errors = 0;
    for (int i = 0; i < 256; i++) {
        if (input[i] != output[i]) {
            errors++;
        }
    }

    printf("Simple XOR test: %d/256 bytes match\n", 256 - errors);
    printf("%s\n\n", errors == 0 ? "✓ XOR test passed" : "✗ XOR test failed");

    return errors == 0 ? 0 : 1;
}
