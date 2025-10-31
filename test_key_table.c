#include <stdio.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern int camellia_keysetup_simd128(struct camellia_simd_ctx *ctx,
                                      const void *key, unsigned int keylen);

int main(void) {
    struct camellia_simd_ctx ctx;
    
    camellia_keysetup_simd128(&ctx, test_key_128, 16);
    
    printf("Key table entries:\n");
    for (int i = 0; i < 26; i++) {
        printf("  key_table[%2d]: %016lx\n", i, ctx.key_table[i]);
    }
    
    return 0;
}
