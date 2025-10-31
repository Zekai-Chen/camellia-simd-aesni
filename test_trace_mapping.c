#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void test_debug_block_order(uint8_t *output, const uint8_t *input);

int main(void) {
    // Create distinctive input blocks
    uint8_t input[256];
    for (int block = 0; block < 16; block++) {
        for (int byte = 0; byte < 16; byte++) {
            input[block * 16 + byte] = (block << 4) | byte;
        }
    }
    
    uint8_t output[256];
    test_debug_block_order(output, input);
    
    printf("Block Mapping After Forward → Reverse Byteslice\n");
    printf("=================================================\n\n");
    
    // Analyze each output position
    for (int out_pos = 0; out_pos < 16; out_pos++) {
        int base = out_pos * 16;
        uint8_t first_byte = output[base];
        int src_block = first_byte >> 4;
        
        printf("Output pos %2d (v%-2d): Block %2d - ", out_pos, out_pos, src_block);
        for (int i = 0; i < 16; i++) {
            printf("%02x ", output[base + i]);
        }
        printf("\n");
    }
    
    printf("\nDe-byteslice param order: v8,v12,v0,v4, v9,v13,v1,v5, v10,v14,v2,v6, v11,v15,v3,v7\n");
    return 0;
}
