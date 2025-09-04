/*
 * Copyright (C) 2024 Camellia AArch64 NEON Test Suite
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#ifdef __aarch64__
#include "camellia_aarch64_neon.h"
#include "camellia-BSD-1.2.0/camellia.h"

/* Test vectors from main.c */
static const uint8_t test_vector_plaintext[] = {
  0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
  0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
};

static const uint8_t test_vector_key_128[] = {
  0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
  0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10
};

static const uint8_t test_vector_ciphertext_128[] = {
  0x67,0x67,0x31,0x38,0x54,0x96,0x69,0x73,
  0x08,0x57,0x06,0x56,0x48,0xea,0xbe,0x43
};

static const uint8_t test_vector_key_192[] = {
  0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
  0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10,
  0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77
};

static const uint8_t test_vector_ciphertext_192[] = {
  0xb4,0x99,0x34,0x01,0xb3,0xe9,0x96,0xf8,
  0x4e,0xe5,0xce,0xe7,0xd7,0x9b,0x09,0xb9
};

static const uint8_t test_vector_key_256[] = {
  0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
  0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10,
  0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
  0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
};

static const uint8_t test_vector_ciphertext_256[] = {
  0x9a,0xcc,0x23,0x7d,0xff,0x16,0xd7,0x6c,
  0x20,0xef,0x7c,0x91,0x9e,0x3a,0x75,0x09
};

/* Reference implementation wrapper */
typedef struct
{
  KEY_TABLE_TYPE table;
  int nbits;
} CAMELLIA_KEY;

static void Camellia_set_key(const void *key, int nbits, CAMELLIA_KEY *ctx)
{
  ctx->nbits = nbits;
  Camellia_Ekeygen(nbits, key, ctx->table);
}

static void Camellia_encrypt(const void *src, void *dst, CAMELLIA_KEY *ctx)
{
  Camellia_EncryptBlock(ctx->nbits, src, ctx->table, dst);
}

static void Camellia_decrypt(const void *src, void *dst, CAMELLIA_KEY *ctx)
{
  Camellia_DecryptBlock(ctx->nbits, src, ctx->table, dst);
}

/* Utility functions */
static double get_time_diff(struct timeval *start, struct timeval *end)
{
  return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1000000.0;
}

static void print_hex(const char *label, const uint8_t *data, size_t len)
{
  printf("%s: ", label);
  for (size_t i = 0; i < len; i++) {
    printf("%02x", data[i]);
  }
  printf("\n");
}

static int verify_test_vector(const uint8_t *key, int key_bits,
                              const uint8_t *plaintext, const uint8_t *expected_ciphertext)
{
  uint8_t ciphertext[16];
  CAMELLIA_KEY ref_key;
  
  Camellia_set_key(key, key_bits, &ref_key);
  Camellia_encrypt(plaintext, ciphertext, &ref_key);
  
  if (memcmp(ciphertext, expected_ciphertext, 16) != 0) {
    printf("ERROR: Test vector mismatch for %d-bit key\n", key_bits);
    print_hex("Expected", expected_ciphertext, 16);
    print_hex("Got", ciphertext, 16);
    return 0;
  }
  return 1;
}

/* Performance benchmark */
static void benchmark_implementation(const char *name, 
                                     void (*encrypt_func)(struct camellia_simd_ctx *, void *, const void *),
                                     int blocks_per_call)
{
  struct camellia_simd_ctx ctx;
  uint8_t plaintext[16 * 32];  /* Max 32 blocks */
  uint8_t ciphertext[16 * 32];
  struct timeval start, end;
  const int iterations = 10000;
  
  /* Initialize key */
  if (camellia_keysetup_neon128(&ctx, test_vector_key_128, 128) != 0) {
    printf("Key setup failed for %s\n", name);
    return;
  }
  
  /* Fill plaintext with test pattern */
  for (int i = 0; i < blocks_per_call; i++) {
    memcpy(plaintext + i * 16, test_vector_plaintext, 16);
  }
  
  printf("Benchmarking %s (%d blocks per call):\n", name, blocks_per_call);
  
  gettimeofday(&start, NULL);
  for (int i = 0; i < iterations; i++) {
    encrypt_func(&ctx, ciphertext, plaintext);
  }
  gettimeofday(&end, NULL);
  
  double elapsed = get_time_diff(&start, &end);
  double total_bytes = (double)iterations * blocks_per_call * 16;
  double throughput_mbs = total_bytes / (elapsed * 1024 * 1024);
  double cycles_per_byte = (elapsed * 2800000000.0) / total_bytes; /* Assume 2.8 GHz */
  
  printf("  Time: %.3f seconds\n", elapsed);
  printf("  Throughput: %.2f MiB/s (%.2f MB/s)\n", 
         throughput_mbs, throughput_mbs * 1024 * 1024 / 1000000);
  printf("  Estimated cycles/byte: %.2f\n", cycles_per_byte);
}

/* SIMD implementation tests */
static void test_neon_implementation(void)
{
  printf("=== AArch64 NEON SIMD Implementation Tests ===\n\n");
  
  /* Check hardware support */
  printf("Hardware feature detection:\n");
  printf("  NEON ASIMD: %s\n", camellia_aarch64_neon_available() ? "Available" : "Not available");
  printf("  Crypto Extensions: %s\n", camellia_aarch64_crypto_available() ? "Available" : "Not available");
  printf("\n");
  
  if (!camellia_aarch64_neon_available()) {
    printf("NEON not available, skipping SIMD tests\n");
    return;
  }
  
  /* Test different S-box variants */
  printf("Testing S-box optimization variants:\n");
  
  const char* variant_names[] = {"Baseline", "Low Register", "Latency Hiding"};
  for (int variant = 0; variant < 3; variant++) {
    printf("\nTesting variant: %s\n", variant_names[variant]);
    camellia_set_sbox_variant((camellia_sbox_variant_t)variant);
    
    struct camellia_simd_ctx ctx;
    uint8_t plaintext_blocks[16 * 16];
    uint8_t ciphertext_blocks[16 * 16];
    
    /* Set up key */
    if (camellia_keysetup_neon128(&ctx, test_vector_key_128, 128) != 0) {
      printf("  Key setup failed\n");
      continue;
    }
    
    /* Fill test blocks */
    for (int i = 0; i < 16; i++) {
      memcpy(plaintext_blocks + i * 16, test_vector_plaintext, 16);
    }
    
    /* Test 16-block parallel encryption */
    camellia_encrypt_16blks_neon128(&ctx, ciphertext_blocks, plaintext_blocks);
    
    /* Verify first block matches reference */
    if (memcmp(ciphertext_blocks, test_vector_ciphertext_128, 16) == 0) {
      printf("  ✓ 16-block parallel encryption: PASS\n");
    } else {
      printf("  ✗ 16-block parallel encryption: FAIL\n");
      print_hex("  Expected", test_vector_ciphertext_128, 16);
      print_hex("  Got", ciphertext_blocks, 16);
    }
    
    /* Performance benchmark for this variant */
    benchmark_implementation("NEON128", camellia_encrypt_16blks_neon128, 16);
  }
}

/* CTR mode test */
static void test_ctr_mode(void)
{
  printf("\n=== CTR Mode Test ===\n");
  
  struct camellia_simd_ctx ctx;
  if (camellia_keysetup_neon128(&ctx, test_vector_key_128, 128) != 0) {
    printf("Key setup failed for CTR test\n");
    return;
  }
  
  const uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                          0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  
  const char *test_message = "Hello, AArch64 NEON Camellia CTR mode!";
  size_t msg_len = strlen(test_message);
  uint8_t encrypted[64];
  uint8_t decrypted[64];
  
  /* Encrypt */
  camellia_ctr_encrypt_neon128(&ctx, iv, (const uint8_t*)test_message, encrypted, msg_len);
  
  /* Decrypt (CTR mode encryption = decryption) */
  camellia_ctr_encrypt_neon128(&ctx, iv, encrypted, decrypted, msg_len);
  decrypted[msg_len] = '\0';
  
  printf("Original:  %s\n", test_message);
  printf("Encrypted: ");
  for (size_t i = 0; i < msg_len; i++) {
    printf("%02x", encrypted[i]);
  }
  printf("\n");
  printf("Decrypted: %s\n", (char*)decrypted);
  
  if (memcmp(test_message, decrypted, msg_len) == 0) {
    printf("CTR mode test: ✓ PASS\n");
  } else {
    printf("CTR mode test: ✗ FAIL\n");
  }
}

int main(void)
{
  printf("Camellia AArch64 NEON SIMD Test Suite\n");
  printf("=====================================\n\n");
  
  /* Verify reference implementation with test vectors */
  printf("Verifying reference implementation:\n");
  if (verify_test_vector(test_vector_key_128, 128, test_vector_plaintext, test_vector_ciphertext_128)) {
    printf("✓ 128-bit key test vector: PASS\n");
  }
  if (verify_test_vector(test_vector_key_192, 192, test_vector_plaintext, test_vector_ciphertext_192)) {
    printf("✓ 192-bit key test vector: PASS\n");
  }
  if (verify_test_vector(test_vector_key_256, 256, test_vector_plaintext, test_vector_ciphertext_256)) {
    printf("✓ 256-bit key test vector: PASS\n");
  }
  printf("\n");
  
  /* Test SIMD implementations */
  test_neon_implementation();
  
  /* Test CTR mode */
  test_ctr_mode();
  
  printf("\n=== Performance Comparison ===\n");
  
  /* Benchmark reference implementation */
  {
    CAMELLIA_KEY ref_key;
    Camellia_set_key(test_vector_key_128, 128, &ref_key);
    
    struct timeval start, end;
    const int iterations = 100000;
    uint8_t plaintext[16], ciphertext[16];
    memcpy(plaintext, test_vector_plaintext, 16);
    
    gettimeofday(&start, NULL);
    for (int i = 0; i < iterations; i++) {
      Camellia_encrypt(plaintext, ciphertext, &ref_key);
    }
    gettimeofday(&end, NULL);
    
    double elapsed = get_time_diff(&start, &end);
    double throughput_mbs = (iterations * 16.0) / (elapsed * 1024 * 1024);
    
    printf("Reference implementation:\n");
    printf("  Throughput: %.2f MiB/s (%.2f MB/s)\n", 
           throughput_mbs, throughput_mbs * 1024 * 1024 / 1000000);
  }
  
#ifdef USE_SIMD256
  /* Benchmark 32-block parallel if available */
  if (camellia_aarch64_neon_available()) {
    benchmark_implementation("NEON256", camellia_encrypt_32blks_neon256, 32);
  }
#endif
  
  printf("\nTest suite completed.\n");
  return 0;
}

#else /* !__aarch64__ */

int main(void)
{
  printf("This test suite requires AArch64 architecture.\n");
  return 1;
}

#endif /* __aarch64__ */