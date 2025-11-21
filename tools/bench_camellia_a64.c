// SPDX-License-Identifier: MIT
/*
 * Camellia AArch64 SIMD Benchmark
 *
 * Measures throughput (MiB/s, MB/s) for 16-way parallel Camellia implementation
 * Supports: enc/dec, 128/192/256-bit keys, batch sizes 1/4/8/16
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#ifndef likely
#  define likely(x)   __builtin_expect(!!(x),1)
#  define unlikely(x) __builtin_expect(!!(x),0)
#endif

// ======== Camellia SIMD API (from camellia_simd.h) ========
#include "../camellia_simd.h"

// C reference implementation (from camellia_simd128_with_aes_instruction_set.c)
#define camellia_encrypt_16blks_simd128 camellia_encrypt_16blks_simd128_c
#define camellia_decrypt_16blks_simd128 camellia_decrypt_16blks_simd128_c
#include "../camellia_simd128_with_aes_instruction_set.c"
#undef camellia_encrypt_16blks_simd128
#undef camellia_decrypt_16blks_simd128

// Assembly implementation is declared in camellia_simd.h and linked from .S

// ======== CLI Parsing ========
typedef enum { IMPL_REF, IMPL_ASM } impl_t;
typedef enum { OP_ENC, OP_DEC } op_t;

static size_t parse_size(const char *s) {
    char *end = NULL;
    double v = strtod(s, &end);
    if (end && (*end=='G' || *end=='g')) return (size_t)(v * 1024 * 1024 * 1024);
    if (end && (*end=='M' || *end=='m')) return (size_t)(v * 1024 * 1024);
    if (end && (*end=='K' || *end=='k')) return (size_t)(v * 1024);
    return (size_t)v;
}

static uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}

static void fill_key(uint8_t *k, int bits) {
    for (int i = 0; i < bits/8; ++i) k[i] = (uint8_t)(i*31 + 7);
}

static void* aligned_alloc_64(size_t n) {
    void *p = NULL;
    if (posix_memalign(&p, 64, (n+63)&~63)) return NULL;
    return p;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [OPTIONS]\n", prog);
    fprintf(stderr, "  --impl <asm|ref>     Implementation (default: asm)\n");
    fprintf(stderr, "  --op <enc|dec>       Operation (default: enc)\n");
    fprintf(stderr, "  --ks <128|192|256>   Key size in bits (default: 128)\n");
    fprintf(stderr, "  --bytes <N|Mi|Gi>    Total bytes to process (default: 2Gi)\n");
    fprintf(stderr, "  --batch <1|4|8|16>   Batch factor (default: 16)\n");
    fprintf(stderr, "                       batch=1: 256B per call (16 blocks)\n");
    fprintf(stderr, "                       batch=16: 4096B per call (256 blocks)\n");
    fprintf(stderr, "  --warm <N>           Warmup iterations (default: 2)\n");
    fprintf(stderr, "  --help               Show this help\n");
}

int main(int argc, char **argv) {
    impl_t impl = IMPL_ASM;
    op_t   op   = OP_ENC;
    int    ks_bits = 128;
    size_t total_bytes = 2ull<<30; // 2GiB
    size_t batch = 16;
    int warm_iters = 2;

    // Parse arguments
    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i],"--impl") && i+1<argc) {
            if (!strcmp(argv[i+1],"asm")) impl=IMPL_ASM;
            else if (!strcmp(argv[i+1],"ref")) impl=IMPL_REF;
            else { fprintf(stderr, "Invalid impl: %s\n", argv[i+1]); return 2; }
            i++;
        } else if (!strcmp(argv[i],"--op") && i+1<argc) {
            if (!strcmp(argv[i+1],"enc")) op=OP_ENC;
            else if (!strcmp(argv[i+1],"dec")) op=OP_DEC;
            else { fprintf(stderr, "Invalid op: %s\n", argv[i+1]); return 2; }
            i++;
        } else if (!strcmp(argv[i],"--ks") && i+1<argc) {
            ks_bits = atoi(argv[++i]);
            if (ks_bits != 128 && ks_bits != 192 && ks_bits != 256) {
                fprintf(stderr, "Invalid key size: %d\n", ks_bits);
                return 2;
            }
        } else if (!strcmp(argv[i],"--bytes") && i+1<argc) {
            total_bytes = parse_size(argv[++i]);
        } else if (!strcmp(argv[i],"--batch") && i+1<argc) {
            batch = (size_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i],"--warm") && i+1<argc) {
            warm_iters = atoi(argv[++i]);
        } else if (!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (!(batch==1 || batch==4 || batch==8 || batch==16)) {
        fprintf(stderr, "batch must be 1/4/8/16\n");
        return 2;
    }

    // IMPORTANT: Each call to camellia_*_16blks_simd128 processes 16 blocks = 256 bytes
    const size_t BYTES_PER_CALL = 256;
    const size_t BYTES_PER_BATCH = BYTES_PER_CALL * batch;

    size_t total_calls = (total_bytes + BYTES_PER_BATCH - 1) / BYTES_PER_BATCH;
    size_t actual_bytes = total_calls * BYTES_PER_BATCH;

    // Allocate buffers (aligned for performance)
    uint8_t *in  = aligned_alloc_64(actual_bytes);
    uint8_t *out = aligned_alloc_64(actual_bytes);
    if (!in || !out) {
        perror("alloc");
        return 1;
    }

    // Fill input with pseudo-random data
    for (size_t i = 0; i < actual_bytes; i++) {
        in[i] = (uint8_t)(i*13u + 3u);
    }

    // Setup key and context
    uint8_t key[32] = {0};
    fill_key(key, ks_bits);
    struct camellia_simd_ctx ctx;
    camellia_keysetup_simd128(&ctx, key, ks_bits);

    // Warmup phase
    for (int w = 0; w < warm_iters; ++w) {
        for (size_t call = 0; call < total_calls; ++call) {
            size_t offset = call * BYTES_PER_BATCH;
            for (size_t b = 0; b < batch; ++b) {
                size_t pos = offset + b * BYTES_PER_CALL;
                if (impl == IMPL_ASM) {
                    if (op == OP_ENC) {
                        camellia_encrypt_16blks_simd128(&ctx, out + pos, in + pos);
                    } else {
                        camellia_decrypt_16blks_simd128(&ctx, out + pos, in + pos);
                    }
                } else {
                    if (op == OP_ENC) {
                        camellia_encrypt_16blks_simd128_c(&ctx, out + pos, in + pos);
                    } else {
                        camellia_decrypt_16blks_simd128_c(&ctx, out + pos, in + pos);
                    }
                }
            }
        }
        // Swap buffers to avoid cache effects
        uint8_t *tmp = in; in = out; out = tmp;
    }

    // Timing phase
    uint64_t t0 = nsec_now();

    for (size_t call = 0; call < total_calls; ++call) {
        size_t offset = call * BYTES_PER_BATCH;
        for (size_t b = 0; b < batch; ++b) {
            size_t pos = offset + b * BYTES_PER_CALL;
            if (impl == IMPL_ASM) {
                if (op == OP_ENC) {
                    camellia_encrypt_16blks_simd128(&ctx, out + pos, in + pos);
                } else {
                    camellia_decrypt_16blks_simd128(&ctx, out + pos, in + pos);
                }
            } else {
                if (op == OP_ENC) {
                    camellia_encrypt_16blks_simd128_c(&ctx, out + pos, in + pos);
                } else {
                    camellia_decrypt_16blks_simd128_c(&ctx, out + pos, in + pos);
                }
            }
        }
    }

    uint64_t t1 = nsec_now();

    // Calculate statistics
    const double sec = (t1 - t0) / 1e9;
    const double mib = (double)actual_bytes / (1024.0*1024.0);
    const double mb  = (double)actual_bytes / 1e6;

    printf("%-4s %-4s ks=%3d batch=%2zu  bytes=%10zu  time=%.6f s\n",
           (impl==IMPL_ASM)?"ASM":"REF",
           (op==OP_ENC)?"ENC":"DEC",
           ks_bits, batch, actual_bytes, sec);
    printf("  throughput: %9.3f MiB/s, %9.3f MB/s\n", mib/sec, mb/sec);

    // Cleanup
    free(in);
    free(out);

    return 0;
}
