/*
 * Copyright (C) 2024 Camellia AArch64 SIMD Implementation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _CAMELLIA_AARCH64_NEON_H_
#define _CAMELLIA_AARCH64_NEON_H_

#include <stdint.h>
#include "camellia_simd.h"

#ifdef __aarch64__
#include <arm_neon.h>

/* Runtime feature detection */
int camellia_aarch64_crypto_available(void);
int camellia_aarch64_neon_available(void);

/* NEON128 vector implementation of key-setup for AArch64 */
int camellia_keysetup_neon128(struct camellia_simd_ctx *ctx, const void *key,
                              unsigned int keylen);

/* NEON128 vector implementation of Camellia for AArch64. 
 * These process 16 blocks in parallel using NEON + Crypto Extensions.
 * IN is pointer to 16 plaintext blocks and OUT is pointer to 16 ciphertext blocks.
 */
void camellia_encrypt_16blks_neon128(struct camellia_simd_ctx *ctx, void *out,
                                     const void *in);
void camellia_decrypt_16blks_neon128(struct camellia_simd_ctx *ctx, void *out,
                                     const void *in);

/* NEON256 vector implementation (32 blocks parallel) */
void camellia_encrypt_32blks_neon256(struct camellia_simd_ctx *ctx, void *out,
                                     const void *in);
void camellia_decrypt_32blks_neon256(struct camellia_simd_ctx *ctx, void *out,
                                     const void *in);

/* S-box optimization variants */
typedef enum {
    CAMELLIA_SBOX_BASELINE,      /* Standard AES-based implementation */
    CAMELLIA_SBOX_LOW_REG,       /* Low register pressure variant */
    CAMELLIA_SBOX_LATENCY_HIDE   /* Latency hiding variant */
} camellia_sbox_variant_t;

void camellia_set_sbox_variant(camellia_sbox_variant_t variant);

/* CTR mode wrapper for benchmarking */
void camellia_ctr_encrypt_neon128(struct camellia_simd_ctx *ctx, 
                                  const uint8_t *iv, const uint8_t *in,
                                  uint8_t *out, size_t len);

#endif /* __aarch64__ */

#endif /* _CAMELLIA_AARCH64_NEON_H_ */