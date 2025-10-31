/*
 * 调试byteslice CD计算
 * 对比C和Assembly的中间状态
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

static const uint8_t test_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

void print_vec(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

// 外部引用C byteslice实现的debug输出
extern uint8_t debug_cd_before_byteslice[128];
extern uint8_t debug_cd_after_byteslice[128];

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];

    // 初始化：16个相同的明文块
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("========================================\n");
    printf("Byteslice CD Debug - C vs Assembly\n");
    printf("========================================\n\n");

    printf("Input plaintext: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", test_input[i]);
    }
    printf("\n\n");

    // 运行C实现
    camellia_keysetup_simd128(&ctx, test_key, 16);

    printf("--- C Implementation ---\n");
    camellia_encrypt_16blks_simd128(&ctx, output, input);

    printf("\nC CD after byteslice (from debug):\n");
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", debug_cd_after_byteslice[i * 16 + j]);
        }
        printf("\n");
    }

    // 运行Assembly实现
    printf("\n--- Assembly Implementation ---\n");
    extern void camellia_encrypt_16blks_simd128_aarch64_asm(
        struct camellia_simd_ctx *ctx,
        void *out, const void *in
    );

    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);

    printf("\nAssembly CD after byteslice (from debug):\n");
    extern uint8_t debug_cd_after_byteslice_asm[128];
    for (int i = 0; i < 8; i++) {
        printf("  CD[%d]: ", i);
        for (int j = 0; j < 16; j++) {
            printf("%02x", debug_cd_after_byteslice_asm[i * 16 + j]);
        }
        printf("\n");
    }

    printf("\n========================================\n");
    printf("Expected: C and Assembly CD should match\n");
    printf("========================================\n");

    return 0;
}
