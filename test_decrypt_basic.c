/*
 * test_decrypt_basic.c - Basic decryption test
 *
 * Test: decrypt(encrypt(plaintext)) == plaintext
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define camellia_encrypt_16blks_simd128 camellia_encrypt_16blks_simd128_c
#define camellia_decrypt_16blks_simd128 camellia_decrypt_16blks_simd128_c
#include "camellia_simd128_with_aes_instruction_set.c"
#undef camellia_encrypt_16blks_simd128
#undef camellia_decrypt_16blks_simd128

/* ASM functions */
extern void camellia_encrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
                                             uint8_t out[256],
                                             const uint8_t in[256]);

extern void camellia_decrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
                                             uint8_t out[256],
                                             const uint8_t in[256]);

static void test_key_length(int key_bits)
{
    struct camellia_simd_ctx ctx;
    uint8_t key[32];
    uint8_t plaintext[256];
    uint8_t ciphertext_c[256], ciphertext_asm[256];
    uint8_t decrypted_c[256], decrypted_asm[256];
    int key_bytes = key_bits / 8;

    /* Initialize key and plaintext */
    for (int i = 0; i < key_bytes; i++) {
        key[i] = i + 1;
    }
    for (int i = 0; i < 256; i++) {
        plaintext[i] = i & 0xff;
    }

    /* Setup key */
    camellia_keysetup_simd128(&ctx, key, key_bits);

    /* Encrypt with C reference */
    camellia_encrypt_16blks_simd128_c(&ctx, ciphertext_c, plaintext);

    /* Encrypt with ASM */
    camellia_encrypt_16blks_simd128(&ctx, ciphertext_asm, plaintext);

    /* Decrypt with C reference */
    camellia_decrypt_16blks_simd128_c(&ctx, decrypted_c, ciphertext_c);

    /* Decrypt with ASM */
    camellia_decrypt_16blks_simd128(&ctx, decrypted_asm, ciphertext_asm);

    /* Verify: decrypted plaintext should match original */
    int errors_c = 0, errors_asm = 0;
    for (int i = 0; i < 256; i++) {
        if (decrypted_c[i] != plaintext[i]) errors_c++;
        if (decrypted_asm[i] != plaintext[i]) errors_asm++;
    }

    /* Also verify ASM encryption matches C */
    int enc_diff = 0;
    for (int i = 0; i < 256; i++) {
        if (ciphertext_asm[i] != ciphertext_c[i]) enc_diff++;
    }

    printf("%d-bit key:\n", key_bits);
    printf("  C   encrypt → decrypt: %s\n", errors_c == 0 ? "✅ PASS" : "❌ FAIL");
    printf("  ASM encrypt → decrypt: %s\n", errors_asm == 0 ? "✅ PASS" : "❌ FAIL");
    printf("  ASM encrypt vs C:     %s\n", enc_diff == 0 ? "✅ MATCH" : "❌ DIFFER");

    if (errors_c > 0) printf("    C decryption errors: %d bytes\n", errors_c);
    if (errors_asm > 0) printf("    ASM decryption errors: %d bytes\n", errors_asm);
    if (enc_diff > 0) printf("    Encryption differs: %d bytes\n", enc_diff);
}

int main(void)
{
    printf("Testing Camellia decryption:\n\n");

    test_key_length(128);
    test_key_length(192);
    test_key_length(256);

    return 0;
}
