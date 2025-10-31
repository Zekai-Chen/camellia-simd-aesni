/*
 * Test: Compare CD values after first roundsm16
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

extern uint8_t debug_ab_before_first_round[128];
extern uint8_t debug_cd_after_byteslice[128];
extern uint8_t debug_cd_after_first_round[128];
extern uint8_t debug_byteslice_input_v8[16];

extern void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in
);

void print_cd_vector(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];

    printf("========================================\n");
    printf("First roundsm16 CD Comparison\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    printf("key_table[0] = 0x%016lx\n", ctx.key_table[0]);
    printf("Expected block 7 XOR key:\n");
    printf("  block 7: ");
    for (int i = 0; i < 16; i++) printf("%02x", test_input[i]);
    printf("\n");

    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("Expected C reference AB before first roundsm16 (after inpack):\n");
    printf("  AB[0]: aeaeaeaeaeaeaeaeaeaeaeaeaeaeaeae\n");
    printf("  AB[1]: 71717171717171717171717171717171\n");
    printf("  AB[2]: c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3c3\n");
    printf("  AB[3]: d5d5d5d5d5d5d5d5d5d5d5d5d5d5d5d5\n");
    printf("  AB[4]: 5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b5b\n");
    printf("  AB[5]: a6a6a6a6a6a6a6a6a6a6a6a6a6a6a6a6\n");
    printf("  AB[6]: bfbfbfbfbfbfbfbfbfbfbfbfbfbfbfbf\n");
    printf("  AB[7]: 1d1d1d1d1d1d1d1d1d1d1d1d1d1d1d1d\n\n");

    printf("Expected C reference CD after first roundsm16:\n");
    printf("  CD[0]: 61616161616161616161616161616161\n");
    printf("  CD[1]: 01010101010101010101010101010101\n");
    printf("  CD[2]: 7e7e7e7e7e7e7e7e7e7e7e7e7e7e7e7e\n");
    printf("  CD[3]: e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0e0\n");
    printf("  CD[4]: dfdfdfdfdfdfdfdfdfdfdfdfdfdfdfdf\n");
    printf("  CD[5]: 61616161616161616161616161616161\n");
    printf("  CD[6]: 68686868686868686868686868686868\n");
    printf("  CD[7]: 60606060606060606060606060606060\n\n");

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);

    printf("Assembly byteslice INPUT v8 (block 7 after pre-whitening):\n");
    print_cd_vector("  v8", debug_byteslice_input_v8);
    printf("  Expected: block 7 XOR key = input[7*16] XOR key\n\n");

    printf("Assembly actual AB before first roundsm16:\n");
    print_cd_vector("  AB[0]", &debug_ab_before_first_round[0]);
    print_cd_vector("  AB[1]", &debug_ab_before_first_round[16]);
    print_cd_vector("  AB[2]", &debug_ab_before_first_round[32]);
    print_cd_vector("  AB[3]", &debug_ab_before_first_round[48]);
    print_cd_vector("  AB[4]", &debug_ab_before_first_round[64]);
    print_cd_vector("  AB[5]", &debug_ab_before_first_round[80]);
    print_cd_vector("  AB[6]", &debug_ab_before_first_round[96]);
    print_cd_vector("  AB[7]", &debug_ab_before_first_round[112]);
    printf("\n");

    printf("Assembly CD after byteslice:\n");
    print_cd_vector("  CD[0]", &debug_cd_after_byteslice[0]);
    print_cd_vector("  CD[1]", &debug_cd_after_byteslice[16]);
    print_cd_vector("  CD[2]", &debug_cd_after_byteslice[32]);
    print_cd_vector("  CD[3]", &debug_cd_after_byteslice[48]);
    print_cd_vector("  CD[4]", &debug_cd_after_byteslice[64]);
    print_cd_vector("  CD[5]", &debug_cd_after_byteslice[80]);
    print_cd_vector("  CD[6]", &debug_cd_after_byteslice[96]);
    print_cd_vector("  CD[7]", &debug_cd_after_byteslice[112]);
    printf("\n");

    printf("Assembly actual CD after first roundsm16:\n");
    print_cd_vector("  CD[0]", &debug_cd_after_first_round[0]);
    print_cd_vector("  CD[1]", &debug_cd_after_first_round[16]);
    print_cd_vector("  CD[2]", &debug_cd_after_first_round[32]);
    print_cd_vector("  CD[3]", &debug_cd_after_first_round[48]);
    print_cd_vector("  CD[4]", &debug_cd_after_first_round[64]);
    print_cd_vector("  CD[5]", &debug_cd_after_first_round[80]);
    print_cd_vector("  CD[6]", &debug_cd_after_first_round[96]);
    print_cd_vector("  CD[7]", &debug_cd_after_first_round[112]);

    printf("\n");

    const uint8_t expected[8][16] = {
        {0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61},
        {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},
        {0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e,0x7e},
        {0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0},
        {0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf,0xdf},
        {0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61},
        {0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68},
        {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60},
    };

    int match = 1;
    for (int i = 0; i < 8; i++) {
        if (memcmp(&debug_cd_after_first_round[i * 16], expected[i], 16) != 0) {
            printf("MISMATCH: CD[%d] does not match!\n", i);
            match = 0;
        }
    }

    if (match) {
        printf("SUCCESS: First roundsm16 produces correct CD values!\n");
        printf("=> Bug is in subsequent rounds or output stage\n");
    } else {
        printf("FAILURE: First roundsm16 produces wrong CD values!\n");
        printf("=> Bug is in: inpack, first roundsm16, or CD storage\n");
    }

    return match ? 0 : 1;
}
