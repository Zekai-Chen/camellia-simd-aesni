/*
 * Copyright (C) 2024 Simple AArch64 NEON Test
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __aarch64__
#include "camellia_aarch64_neon.h"
#include "camellia-BSD-1.2.0/camellia.h"

/* Simple test vectors */
static const uint8_t test_key[16] = {
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
};

static const uint8_t test_plaintext[16] = {
    0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
    0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
};

static const uint8_t expected_ciphertext[16] = {
    0x67,0x67,0x31,0x38,0x54,0x96,0x69,0x73,
    0x08,0x57,0x06,0x56,0x48,0xea,0xbe,0x43
};

int main(void)
{
    printf("Simple AArch64 NEON Camellia Test\n");
    printf("==================================\n\n");
    
    /* Check hardware support */
    printf("Hardware Features:\n");
    printf("  NEON Support: %s\n", camellia_aarch64_neon_available() ? "Yes" : "No");
    printf("  Crypto Extensions: %s\n", camellia_aarch64_crypto_available() ? "Yes" : "No");
    printf("\n");
    
    /* Test reference implementation first */
    printf("Testing reference implementation:\n");
    KEY_TABLE_TYPE key_table;
    uint8_t result[16];
    
    Camellia_Ekeygen(128, test_key, key_table);
    Camellia_EncryptBlock(128, test_plaintext, key_table, result);
    
    if (memcmp(result, expected_ciphertext, 16) == 0) {
        printf("  ✓ Reference implementation: PASS\n");
    } else {
        printf("  ✗ Reference implementation: FAIL\n");
        return 1;
    }
    
    /* Test NEON implementation if available */
    if (camellia_aarch64_neon_available()) {
        printf("\nTesting NEON implementation:\n");
        
        struct camellia_simd_ctx ctx;
        if (camellia_keysetup_neon128(&ctx, test_key, 128) == 0) {
            printf("  ✓ Key setup: PASS\n");
            
            /* Test single block via 16-block interface */
            uint8_t neon_input[16 * 16];
            uint8_t neon_output[16 * 16];
            
            memset(neon_input, 0, sizeof(neon_input));
            memcpy(neon_input, test_plaintext, 16);
            
            camellia_encrypt_16blks_neon128(&ctx, neon_output, neon_input);
            
            if (memcmp(neon_output, expected_ciphertext, 16) == 0) {
                printf("  ✓ NEON 16-block encryption: PASS\n");
            } else {
                printf("  ✗ NEON 16-block encryption: FAIL\n");
                printf("    Expected: ");
                for (int i = 0; i < 16; i++) printf("%02x", expected_ciphertext[i]);
                printf("\n    Got:      ");
                for (int i = 0; i < 16; i++) printf("%02x", neon_output[i]);
                printf("\n");
            }
        } else {
            printf("  ✗ Key setup: FAIL\n");
        }
    } else {
        printf("\nNEON not available - skipping SIMD tests\n");
    }
    
    printf("\nTest completed.\n");
    return 0;
}

#else /* !__aarch64__ */

int main(void)
{
    printf("This test requires AArch64 architecture.\n");
    return 1;
}

#endif /* __aarch64__ */