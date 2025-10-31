#include <stdio.h>
#include <stdint.h>

extern void test_swap_halves(uint8_t *output, const uint8_t *input);

int main(void) {
    uint8_t input[256], output[256];
    
    // Distinctive pattern
    for (int i = 0; i < 256; i++) {
        input[i] = i;
    }
    
    test_swap_halves(output, input);
    
    printf("After byteslice roundtrip WITH 64-bit half swap:\n\n");
    
    for (int pos = 0; pos < 16; pos++) {
        int base = pos * 16;
        printf("Pos %2d: ", pos);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output[base + i]);
        }
        
        // Check if it matches expected input block
        int expected_block = pos;
        int matches = 1;
        for (int i = 0; i < 16; i++) {
            if (output[base + i] != expected_block * 16 + i) {
                matches = 0;
                break;
            }
        }
        printf("%s\n", matches ? " ✓ CORRECT!" : "");
    }
    
    return 0;
}
