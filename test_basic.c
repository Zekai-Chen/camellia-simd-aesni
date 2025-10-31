/*
 * Basic correctness test for assembly implementation
 */
#include <stdio.h>
#include <string.h>
#include "camellia_simd.h"

// Test key and data
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_plaintext[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t plaintext[256];  // 16 blocks
    uint8_t ciphertext_c[256];
    uint8_t ciphertext_asm[256];

    printf("Testing Camellia assembly implementation...\n\n");

    // Initialize key
    printf("1. Setting up key...\n");
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("   FAILED: Key setup failed\n");
        return 1;
    }
    printf("   OK: Key setup successful\n");

    // Fill plaintext (repeat test pattern 16 times)
    printf("2. Preparing plaintext...\n");
    for (int i = 0; i < 16; i++) {
        memcpy(plaintext + i * 16, test_plaintext, 16);
    }
    printf("   OK: Plaintext prepared\n");

    // Test C implementation
    printf("3. Testing C intrinsics implementation...\n");
    camellia_encrypt_16blks_simd128(&ctx, ciphertext_c, plaintext);
    printf("   OK: C implementation completed\n");
    printf("   First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", ciphertext_c[i]);
    }
    printf("\n");

    // Test assembly implementation
    printf("4. Testing assembly implementation...\n");
    printf("   About to call assembly function...\n");
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, ciphertext_asm, plaintext);
    printf("   OK: Assembly implementation completed\n");
    printf("   First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", ciphertext_asm[i]);
    }
    printf("\n");

    // Compare results
    printf("5. Comparing results...\n");
    if (memcmp(ciphertext_c, ciphertext_asm, 256) == 0) {
        printf("   SUCCESS: Assembly output matches C implementation!\n");
        return 0;
    } else {
        printf("   FAILED: Outputs differ\n");
        printf("   Differences:\n");
        for (int i = 0; i < 256; i++) {
            if (ciphertext_c[i] != ciphertext_asm[i]) {
                printf("     Byte %d: C=0x%02x ASM=0x%02x\n",
                       i, ciphertext_c[i], ciphertext_asm[i]);
            }
        }
        return 1;
    }
}
