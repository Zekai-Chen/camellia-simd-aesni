/*
 * 验证汇编在byteslice前的AB和CD寄存器值
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

extern void camellia_encrypt_16blks_simd128_aarch64_asm(struct camellia_simd_ctx *ctx,
                                                          void *out, const void *in);

void print_hex(const char *label, const uint8_t *data) {
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
    printf("验证汇编寄存器值\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("C reference values before outunpack:\n");
    printf("  AB x0: 08080808080808080808080808080808\n");
    printf("  AB x7: 43434343434343434343434343434343\n");
    printf("  CD x8: c8c8c8c8c8c8c8c8c8c8c8c8c8c8c8c8\n");
    printf("  CD x15: 8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a\n\n");

    // 运行汇编（它会在 sp+256 处保存寄存器值）
    camellia_encrypt_16blks_simd128_aarch64_asm(&ctx, output, input);

    // 注意：由于栈已经返回，我们无法直接读取sp+256的值
    // 我需要修改汇编，让它返回这些值

    printf("Assembly output (first block): ");
    for (int i = 0; i < 16; i++) {
        printf("%02x", output[i]);
    }
    printf("\n");

    return 0;
}
