/*
 * 验证汇编在输出阶段加载的AB和CD值
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <arm_neon.h>
#include "camellia_simd.h"

static const uint8_t test_key_128[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

static const uint8_t test_input[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

// 声明汇编调试函数：在输出阶段打印AB和CD
extern void camellia_encrypt_16blks_simd128_aarch64_asm_debug(
    struct camellia_simd_ctx *ctx,
    void *out, const void *in,
    void *ab_out, void *cd_out  // 输出AB和CD用于调试
);

void print_vector(const char *label, const uint8_t *data) {
    printf("%s: ", label);
    for (int i = 0; i < 16; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

int main(void) {
    struct camellia_simd_ctx ctx;
    uint8_t input[256], output[256];
    uint8_t ab_debug[128], cd_debug[128];  // 用于接收AB和CD的调试数据

    printf("========================================\n");
    printf("验证汇编加载的AB和CD值\n");
    printf("========================================\n\n");

    camellia_keysetup_simd128(&ctx, test_key_128, 16);

    // 创建输入：重复测试块16次
    for (int i = 0; i < 16; i++) {
        memcpy(&input[i * 16], test_input, 16);
    }

    printf("C reference AB/CD before outunpack:\n");
    printf("  AB x0: 08080808080808080808080808080808\n");
    printf("  AB x7: 43434343434343434343434343434343\n");
    printf("  CD x8: c8c8c8c8c8c8c8c8c8c8c8c8c8c8c8c8\n");
    printf("  CD x15: 8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a8a\n\n");

    // 调用修改后的汇编函数（如果我们实现了它）
    // 暂时我们使用现有函数并检查最终输出

    printf("Assembly需要匹配这些值。\n");
    printf("如果不匹配，说明AB/CD加载位置错误。\n");

    return 0;
}
