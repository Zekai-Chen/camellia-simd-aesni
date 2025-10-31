# Kivilinna's Camellia SIMD Implementation - 深度分析

## 概述
这是对 `camellia_simd128_with_aes_instruction_set.c` 的详细分析，帮助理解如何实现真正的16块并行SIMD加密。

## 1. 整体架构

### 1.1 跨平台宏系统
Kivilinna使用了一个巧妙的宏系统来支持多个平台：
- **x86/x86-64**: AES-NI + SSE4.1/AVX
- **AArch64**: NEON + Crypto Extensions
- **PowerPC**: VSX + Crypto instructions

**关键设计**：所有平台特定的intrinsics都被抽象成统一的宏名称。

### AArch64宏定义 (122-201行)

```c
// 基础类型定义
#define __m128i uint64x2_t  // 统一使用uint64x2_t作为128位向量类型

// 逻辑运算
#define vpand128(a, b, o)       (o = vandq_u64(b, a))      // AND
#define vpxor128(a, b, o)       (o = veorq_u64(b, a))      // XOR
#define vpor128(a, b, o)        (o = vorrq_u64(b, a))      // OR
#define vpandn128(a, b, o)      (o = vbicq_u64(a, b))      // AND NOT

// 位移操作
#define vpsrlb128(s, a, o)      (o = (__m128i)vshrq_n_u8((uint8x16_t)a, s))  // 字节右移
#define vpsllb128(s, a, o)      (o = (__m128i)vshlq_n_u8((uint8x16_t)a, s))  // 字节左移

// 字节重排 (关键操作!)
#define vpshufb128(m, a, o)     (o = (__m128i)vqtbl1q_u8((uint8x16_t)a, (uint8x16_t)m))

// AES指令 (核心加速!)
#define vaesenclast128(a, b, o) (o = (__m128i)vaeseq_u8((uint8x16_t)b, (uint8x16_t)a))
```

**重要说明**：`vaeseq_u8` 执行 AES SubBytes + ShiftRows + MixColumns (但XOR部分要手动)

## 2. 核心技术：Byte-Slicing (字节切片)

### 2.1 什么是Byte-Slicing？

**传统方式**（每个向量存一个完整块）：
```
block0: [b0  b1  b2  ... b15] ← 128位向量
block1: [b0  b1  b2  ... b15] ← 128位向量
...
block15: [b0  b1  b2  ... b15] ← 128位向量
```

**Byte-Slicing方式**（每个向量存16个块的同一字节位置）：
```
x0: [block0.b0, block1.b0, ..., block15.b0] ← 16个块的第0字节
x1: [block0.b1, block1.b1, ..., block15.b1] ← 16个块的第1字节
...
x15: [block0.b15, block1.b15, ..., block15.b15] ← 16个块的第15字节
```

### 2.2 为什么使用Byte-Slicing？

1. **S-box可以并行**：对16个字节同时查表/变换
2. **位操作更高效**：XOR/AND/OR等操作天然并行
3. **减少内存访问**：数据在寄存器中重新组织，减少cache miss

### 2.3 转换过程：`byteslice_16x16b_fast` (702-749行)

这个宏将16个128位块转换为byte-sliced格式：

```c
#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1,
                              a2, b2, c2, d2, a3, b3, c3, d3, st0, st1)
    // 1. 4x4转置 (分4组进行)
    transpose_4x4(a0, a1, a2, a3, d2, d3);
    transpose_4x4(b0, b1, b2, b3, d2, d3);
    transpose_4x4(c0, c1, c2, d3, a0, a1);
    transpose_4x4(d0, d1, d2, d3, a0, a1);

    // 2. 字节shuffle重排
    vpshufb128(shufb_mask, a2, a2);
    // ... 对每个向量执行

    // 3. 再次4x4转置完成转换
    transpose_4x4(a0, b0, c0, d0, d2, d3);
    // ...
```

**核心思想**：通过多次转置+shuffle，将按块组织的数据重排为按字节位置组织。

## 3. S-box加速实现

### 3.1 核心宏：`filter_8bit` (280-288行)

```c
#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0)
    vpand128(x, mask4bit, tmp0);           // 提取低4位
    vpsrlb128(4, x, x);                     // 右移4位得到高4位

    vpshufb128(tmp0, lo_t, tmp0);          // 低4位查表
    vpshufb128(x, hi_t, x);                // 高4位查表
    vpxor128(tmp0, x, x);                  // 合并结果
```

**工作原理**：
1. 将8位输入分解为高4位和低4位
2. 分别用查找表（16字节）进行映射
3. XOR合并结果

这比256字节的查找表更cache友好！

### 3.2 S-box完整流程：`roundsm16` (355-492行)

```c
#define roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, ...)
    // 1. 预变换 (Camellia → AES域)
    filter_8bit(x0, pre_tf_lo_s1, pre_tf_hi_s1, mask_0f, t6);
    filter_8bit(x7, pre_tf_lo_s1, pre_tf_hi_s1, mask_0f, t6);
    // ... 对8个字节位置执行

    // 2. AES SubBytes (硬件加速!)
    aes_subbytes_and_shuf_and_xor(zero, x0, x0);  // ← vaeseq_u8
    aes_subbytes_and_shuf_and_xor(zero, x7, x7);
    // ...

    // 3. 后变换 (AES域 → Camellia域)
    filter_8bit(x0, post_tf_lo_s1, post_tf_hi_s1, mask_0f, t6);
    // ...

    // 4. P-function (线性变换 - 大量XOR)
    vpxor128(x5, x0, x0);
    vpxor128(x6, x1, x1);
    // ... 26行XOR操作实现Camellia的P-function

    // 5. 添加轮密钥
    vpxor128(key_byte0, x0, x0);
    // ...
```

**关键点**：
- **Camellia的4个S-box** (S1, S2, S3, S4) 都有不同的预/后变换表
- **S4需要额外处理**：输入要循环左移1位 (<<<1)
- **使用AES指令加速中间的SubBytes操作**

## 4. 16块并行加密流程

### 4.1 主函数：`camellia_encrypt_16blks_simd128` (1025-1079行)

```c
void camellia_encrypt_16blks_simd128(struct camellia_simd_ctx *ctx,
                                     void *vout, const void *vin)
{
    __m128i x0-x15;  // 16个向量寄存器
    __m128i ab[8];   // AB状态 (64位)
    __m128i cd[8];   // CD状态 (64位)

    // 1. 准备常量表 (加载到栈上以便快速访问)
    prepare_frequent_constants();

    // 2. 输入打包 + 初始密钥白化
    inpack16_pre(x0-x15, in, ctx->key_table[0]);

    // 3. 转换为byte-sliced格式
    inpack16_post(x0-x15, ab, cd);

    // 4. 加密轮次
    while (1) {
        enc_rounds16(x0-x15, ab, cd, k);  // 6轮Feistel

        if (k == lastk - 8) break;

        fls16(ab, cd, ...);  // FL/FL^-1 层

        k += 8;
    }

    // 5. 输出解包 + 最终密钥白化
    outunpack16(x0-x15, ctx->key_table[lastk], ...);

    // 6. 写回输出
    write_output(x0-x15, out);
}
```

### 4.2 加密轮次：`enc_rounds16` (532-557行)

```c
#define enc_rounds16(x0-x15, ab, cd, k)
    // 执行6轮Feistel结构
    roundsm16(x0-x7, ..., cd, ctx->key_table[k+0]);  // 轮1
    roundsm16(x8-x15, ..., ab, ctx->key_table[k+1]); // 轮2
    roundsm16(x0-x7, ..., cd, ctx->key_table[k+2]);  // 轮3
    roundsm16(x8-x15, ..., ab, ctx->key_table[k+3]); // 轮4
    roundsm16(x0-x7, ..., cd, ctx->key_table[k+4]);  // 轮5
    roundsm16(x8-x15, ..., ab, ctx->key_table[k+5]); // 轮6
```

**Camellia结构**：
- 128位块 = AB (64位) + CD (64位)
- 每轮：新CD = F(AB, key) ⊕ 旧CD，新AB = 旧CD
- 6轮为一组，中间插入FL/FL^-1层

### 4.3 数据流示意

```
输入: 16个128位明文块 (256字节)
   ↓
[inpack16_pre] 从内存加载 + XOR初始密钥
   ↓
[byteslice_16x16b] 转换为byte-sliced格式
   x0 = [blk0.b0, blk1.b0, ..., blk15.b0]
   x1 = [blk0.b1, blk1.b1, ..., blk15.b1]
   ...
   ↓
[enc_rounds16] × N  执行多组6轮加密
   每轮：
   - 预变换 (filter_8bit)
   - AES SubBytes (vaeseq_u8) ← 硬件加速!
   - 后变换 (filter_8bit)
   - P-function (XOR网络)
   - 添加轮密钥
   ↓
[FL层] 密钥相关的线性/非线性变换
   ↓
[outunpack16] 转回普通格式 + XOR最终密钥
   ↓
输出: 16个128位密文块
```

## 5. 性能关键点

### 5.1 为什么这么快？

1. **真正的16块并行**：所有操作同时处理16个块
2. **硬件加速S-box**：使用`vaeseq_u8`而不是查找表
3. **寄存器密集型**：数据常驻寄存器，减少内存访问
4. **常量预加载**：查找表加载到栈（L1 cache）
5. **向量化P-function**：XOR操作天然向量化

### 5.2 寄存器使用

AArch64有32个128位NEON寄存器：
- `x0-x15`: 16个主工作寄存器
- `ab[8]`, `cd[8]`: 临时状态存储（使用栈内存）
- `t0-t7`: 临时寄存器用于变换

### 5.3 内存访问模式

```c
// 输入：线性读取256字节
vmovdqu128_memld(in + 0*16, x0);
vmovdqu128_memld(in + 1*16, x1);
// ...
// ↓ 在寄存器中重组为byte-sliced
// ↓ 所有计算在寄存器中完成
// ↓ 重组回普通格式
// 输出：线性写入256字节
vmovdqu128_memst(x0, out + 0*16);
vmovdqu128_memst(x1, out + 1*16);
```

**优化**：
- 使用非对齐加载/存储 (`vmovdqu`)
- 只在输入/输出时访问内存
- 中间计算完全在寄存器中

## 6. 与你之前实现的对比

### 你的实现 (`camellia_aarch64_neon.c`)

```c
void camellia_encrypt_16blks_neon128(...) {
    for (int i = 0; i < 16; i++) {
        Camellia_EncryptBlock(...);  // ← 逐块加密！
    }
}
```

**问题**：
- ❌ 没有并行处理
- ❌ 没有byte-slicing
- ❌ 没有利用SIMD
- ❌ 性能不可能提升

### Kivilinna的实现

```c
void camellia_encrypt_16blks_simd128(...) {
    byteslice_16x16b(...);  // 转换为SIMD格式
    enc_rounds16(...);       // 16块同时处理
    outunpack16(...);        // 转回普通格式
}
```

**优势**：
- ✅ 真正的16块并行
- ✅ 使用byte-slicing技术
- ✅ 充分利用SIMD指令
- ✅ 实际3.5-4x加速

## 7. 关键要点总结

1. **不是简单的intrinsics重写**：需要理解byte-slicing、数据重组、向量化算法设计

2. **宏系统很复杂**：355-492行的`roundsm16`宏包含整个S-box+P-function实现

3. **需要理解Camellia算法**：
   - Feistel结构
   - 4个不同的S-box
   - FL/FL^-1层
   - P-function (线性变换)

4. **AES指令的巧妙使用**：
   - `vaeseq_u8` = SubBytes + ShiftRows + MixColumns
   - 用预/后变换表映射Camellia S-box到AES SubBytes
   - 需要undo ShiftRows (因为Camellia不需要)

5. **2000行代码的原因**：
   - 跨平台宏定义 (~300行)
   - S-box变换表定义 (~200行)
   - 复杂的宏定义 (roundsm16, enc_rounds16, byteslice等 ~600行)
   - 密钥调度实现 (~500行)
   - 辅助函数和常量 (~400行)

## 8. 下一步建议

如果要继续这个项目，你需要：

1. **完全理解这个文件**：逐宏、逐函数分析
2. **手动验证小段代码**：写测试验证每个组件
3. **渐进式移植**：
   - 先让byte-slicing工作
   - 再实现filter_8bit
   - 再实现roundsm16
   - 最后组装完整的enc_rounds16
4. **每步验证正确性**：与参考实现对比中间结果
5. **考虑是否需要汇编**：现代编译器已经很好了，C intrinsics可能足够

**诚实建议**：这是一个需要几周时间、逐步完成的工作，不是AI可以一次性"生成"的。
