#!/bin/bash
# 调试单个块的加密，验证基础功能

echo "🔧 Single Block Encryption Debug"
echo "================================"

# 创建一个简单的单块测试程序
cat > debug_single.c << 'EOF'
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// C reference实现 (简化版本，仅用于调试)
extern void camellia_encrypt_32blks_c_reference(const uint8_t *in, uint8_t *out, const uint8_t *key);
extern void camellia_encrypt_32blks_aarch64_neon_crypto(const uint8_t *in, uint8_t *out, const uint8_t *key);

void print_hex(const char* label, const uint8_t* data, int len) {
    printf("%s: ", label);
    for(int i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if((i + 1) % 8 == 0) printf(" ");
    }
    printf("\n");
}

int main() {
    // 测试用密钥 (128位)
    uint8_t key[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    
    // 输入数据 (32个块 = 512字节)
    uint8_t input[512];
    uint8_t output_ref[512];
    uint8_t output_simd[512];
    
    // 初始化输入数据
    for(int i = 0; i < 512; i++) {
        input[i] = 0xed + (i % 256);
    }
    
    printf("🔍 Testing with standard test vectors\n");
    printf("------------------------------------\n");
    
    print_hex("Key", key, 16);
    print_hex("Input (first 32 bytes)", input, 32);
    
    // 运行C reference
    printf("\n🧮 Running C Reference...\n");
    camellia_encrypt_32blks_c_reference(input, output_ref, key);
    print_hex("C Ref Output (first 32 bytes)", output_ref, 32);
    
    // 运行SIMD版本
    printf("\n🚀 Running Assembly SIMD...\n");
    camellia_encrypt_32blks_aarch64_neon_crypto(input, output_simd, key);
    print_hex("SIMD Output (first 32 bytes)", output_simd, 32);
    
    // 比较结果
    printf("\n🔍 Comparison Result:\n");
    if(memcmp(output_ref, output_simd, 512) == 0) {
        printf("✅ PERFECT MATCH! SIMD implementation is correct.\n");
        return 0;
    } else {
        printf("❌ MISMATCH! SIMD implementation has bugs.\n");
        
        // 找到第一个不同的字节
        for(int i = 0; i < 512; i++) {
            if(output_ref[i] != output_simd[i]) {
                printf("First difference at byte %d: ref=0x%02x, simd=0x%02x\n", 
                       i, output_ref[i], output_simd[i]);
                break;
            }
        }
        return 1;
    }
}
EOF

echo "🔧 Compiling debug program..."
gcc -o debug_single debug_single.c camellia_aarch64_neon_crypto.o -static

echo "🚀 Running debug test..."
./debug_single

echo ""
echo "🔍 This will help us understand if the issue is:"
echo "1. Wrong key expansion"
echo "2. Wrong round function implementation" 
echo "3. Wrong data layout/endianness"
echo "4. Wrong byteslicing implementation"