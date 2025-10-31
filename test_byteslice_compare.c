#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>

// Test input: 16 blocks after pre-whitening
static uint8_t input[256];

void test_c_byteslice(uint8_t out_ab[128], uint8_t out_cd[128]) {
    // C byteslice implementation - extracted from reference
    extern void c_byteslice_test(uint8_t *in, uint8_t *ab, uint8_t *cd);
    c_byteslice_test(input, out_ab, out_cd);
}

extern void test_asm_byteslice(uint8_t *in, uint8_t *ab, uint8_t *cd);

int main() {
    uint8_t ab_c[128], cd_c[128];
    uint8_t ab_asm[128], cd_asm[128];
    
    // Initialize input: all same for visibility
    for (int i = 0; i < 256; i++) input[i] = i;
    
    // Test C
    test_c_byteslice(ab_c, cd_c);
    
    // Test ASM  
    test_asm_byteslice(input, ab_asm, cd_asm);
    
    printf("C CD[0]: ");
    for (int i = 0; i < 16; i++) printf("%02x", cd_c[i]);
    printf("\n");
    
    printf("ASM CD[0]: ");
    for (int i = 0; i < 16; i++) printf("%02x", cd_asm[i]);
    printf("\n");
    
    if (memcmp(cd_c, cd_asm, 128) == 0) {
        printf("CD MATCH!\n");
        return 0;
    } else {
        printf("CD MISMATCH!\n");
        for (int i = 0; i < 8; i++) {
            printf("C  CD[%d]: ", i);
            for (int j = 0; j < 16; j++) printf("%02x", cd_c[i*16+j]);
            printf("\n");
            printf("ASM CD[%d]: ", i);
            for (int j = 0; j < 16; j++) printf("%02x", cd_asm[i*16+j]);
            printf("\n");
        }
        return 1;
    }
}
