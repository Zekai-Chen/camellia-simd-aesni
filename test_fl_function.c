/*
 * Test: Verify FL function (fls16) works correctly
 * This tests the FL layer in isolation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

// Test key for predictable key schedule
static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static void print_hex(const char *label, const uint8_t *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

static void print_vector(const char *label, const uint8x16_t v) {
    uint8_t buf[16];
    vst1q_u8(buf, v);
    print_hex(label, buf, 16);
}

// Simple FL implementation for reference
// FL operates on 64-bit halves: L (left) and R (right)
// In byte-sliced format:
//   L is split into ll (low 32) and lr (high 32)
//   R is split into rl (low 32) and rr (high 32)
static void fl_reference_scalar(uint64_t *L, uint64_t *R,
                                 const uint64_t kl, const uint64_t kr) {
    uint32_t ll = (uint32_t)(*L);
    uint32_t lr = (uint32_t)(*L >> 32);
    uint32_t rl = (uint32_t)(*R);
    uint32_t rr = (uint32_t)(*R >> 32);

    uint32_t kll = (uint32_t)(kl);
    uint32_t klr = (uint32_t)(kl >> 32);
    uint32_t krl = (uint32_t)(kr);
    uint32_t krr = (uint32_t)(kr >> 32);

    // Step 1: t = kll & ll; lr ^= rol32(t, 1)
    uint32_t t = kll & ll;
    lr ^= ((t << 1) | (t >> 31));

    // Step 2: t = krr | rr; rl ^= t
    t = krr | rr;
    rl ^= t;

    // Step 3: t = krl & rl; rr ^= rol32(t, 1)
    t = krl & rl;
    rr ^= ((t << 1) | (t >> 31));

    // Step 4: t = klr | lr; ll ^= t
    t = klr | lr;
    ll ^= t;

    *L = ((uint64_t)lr << 32) | ll;
    *R = ((uint64_t)rr << 32) | rl;
}

int main(void) {
    struct camellia_simd_ctx ctx;

    printf("========================================\n");
    printf("FL Function Test\n");
    printf("========================================\n\n");

    // Setup key
    if (camellia_keysetup_simd128(&ctx, test_key_128, 16) != 0) {
        printf("ERROR: Key setup failed\n");
        return 1;
    }

    // For 128-bit key, FL is called with keys k+8 and k+9
    // First FL: keys 8 and 9
    // Second FL: keys 16 and 17
    uint64_t kl = ctx.key_table[8];
    uint64_t kr = ctx.key_table[9];

    printf("FL Keys (first FL layer):\n");
    printf("  key_table[8]  (kl): 0x%016lx\n", kl);
    printf("  key_table[9]  (kr): 0x%016lx\n\n", kr);

    // Test with simple known values
    uint64_t L = 0x0123456789abcdefULL;
    uint64_t R = 0xfedcba9876543210ULL;

    printf("Input:\n");
    printf("  L: 0x%016lx\n", L);
    printf("  R: 0x%016lx\n\n", R);

    // Apply FL using reference implementation
    uint64_t L_ref = L, R_ref = R;
    fl_reference_scalar(&L_ref, &R_ref, kl, kr);

    printf("Expected Output (scalar reference):\n");
    printf("  L: 0x%016lx\n", L_ref);
    printf("  R: 0x%016lx\n\n", R_ref);

    // Now test with 16 identical blocks to match byte-slicing
    // In byte-sliced format, each vector contains the same byte position from all 16 blocks
    // Since all blocks are identical, each vector will have 16 copies of the same byte

    // Create byte-sliced input (simulated)
    // For a single 128-bit block split as L (bytes 0-7) and R (bytes 8-15):
    //   L bytes: 01 23 45 67 89 ab cd ef
    //   R bytes: fe dc ba 98 76 54 32 10
    // After byteslice with interleaving (pattern 0,2,1,3 for each 32-bit group):
    //   l0 = byte 0 = 0x01 (repeated 16 times)
    //   l1 = byte 2 = 0x45 (repeated 16 times)
    //   l2 = byte 1 = 0x23 (repeated 16 times)
    //   l3 = byte 3 = 0x67 (repeated 16 times)
    //   l4 = byte 4 = 0x89 (repeated 16 times)
    //   l5 = byte 6 = 0xcd (repeated 16 times)
    //   l6 = byte 5 = 0xab (repeated 16 times)
    //   l7 = byte 7 = 0xef (repeated 16 times)
    // Similarly for R (r0-r7)

    printf("Testing byte-sliced format...\n");
    printf("(This would require assembly test or C SIMD implementation)\n\n");

    // For now, the scalar reference gives us expected values
    // The assembly test would need to:
    // 1. Create byte-sliced vectors with known values
    // 2. Call fls16 from assembly
    // 3. De-byteslice the result
    // 4. Compare with scalar reference

    printf("Next step: Create assembly wrapper to test fls16 directly\n");
    printf("This requires:\n");
    printf("  1. Prepare byte-sliced AB and CD vectors\n");
    printf("  2. Call fls16 from assembly\n");
    printf("  3. Extract results and compare\n");

    return 0;
}
