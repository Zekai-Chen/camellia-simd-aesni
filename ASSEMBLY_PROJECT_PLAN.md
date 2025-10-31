# AArch64 汇编实现项目计划

## ✅ 项目清理完成

**删除的文件（假的C实现）**：
- ❌ `camellia_aarch64_neon.c` - 循环调用标量实现（已删除）
- ❌ `camellia_aarch64_neon.h` - 对应头文件（已删除）

**保留的文件（真实工作的基础）**：
- ✅ `camellia_simd128_aarch64_neon_crypto.S` - 汇编框架
- ✅ `tests/test_*.c` - 组件验证测试

---

## 🎯 项目目标：手写汇编实现

### 为什么要用汇编？

**合理的理由**：
1. **对标x86汇编版本**：
   - `camellia_simd128_x86-64_aesni_avx.S` (49KB, 汇编实现)
   - `camellia_simd256_x86-64_aesni_avx2.S` (30KB, 汇编实现)
   - x86有汇编版本，AArch64也应该有

2. **可能的性能提升**：
   - 更精确的指令调度
   - 更好的寄存器分配
   - 减少编译器开销
   - **但Iakov警告过：现代编译器很好，提升可能只有5-10%**

3. **学习价值**：
   - 深入理解AArch64 SIMD
   - 掌握NEON和Crypto Extensions
   - 理解底层优化技术

---

## 📊 现状分析

### 已有的实现

| 实现 | 类型 | 状态 | 性能 |
|------|------|------|------|
| `camellia_simd128_with_aes_instruction_set.c` (Kivilinna) | C intrinsics | ✅ 完整工作 | 613 MiB/s (3.6x) |
| `camellia_simd128_x86-64_aesni_avx.S` (Kivilinna) | x86汇编 | ✅ 完整工作 | 更快（在x86上） |
| `camellia_simd128_aarch64_neon_crypto.S` (你) | AArch64汇编 | ⏳ 框架，未实现 | 待完成 |

### C intrinsics vs 汇编

**C intrinsics版本**（已工作）：
```c
// camellia_simd128_with_aes_instruction_set.c
void camellia_encrypt_16blks_simd128(...) {
    // 完整的byte-slicing实现
    inpack16_pre(...);
    byteslice_16x16b(...);
    enc_rounds16(...);
    // ...
}
```

**汇编版本**（你要做的）：
```asm
// camellia_simd128_aarch64_neon_crypto.S
.global camellia_encrypt_16blks_simd128_aarch64_asm
camellia_encrypt_16blks_simd128_aarch64_asm:
    // 目标：用纯汇编实现相同功能
    // 可能比C intrinsics快5-10%
```

---

## 🚧 工作范围

### 阶段1：研究和理解（预计3-5天）

**任务**：
1. ✅ 理解Kivilinna的C实现（已完成）
   - ✅ Byte-slicing技术
   - ✅ filter_8bit实现
   - ✅ 转置操作

2. 研究x86汇编参考
   ```bash
   # 分析x86汇编版本的结构
   less camellia_simd128_x86-64_aesni_avx.S

   # 关键部分：
   # - 寄存器分配策略
   # - S-box实现（使用AESNI）
   # - 数据重组
   # - 循环展开
   ```

3. 映射x86指令到AArch64
   ```
   x86指令              →  AArch64指令
   ────────────────────────────────────
   vaesenclast %xmm0    →  aese v0.16b, v31.16b
   vpshufb %xmm1, %xmm0 →  tbl v0.16b, {v0.16b}, v1.16b
   vpxor %xmm1, %xmm0   →  eor v0.16b, v0.16b, v1.16b
   vpand %xmm1, %xmm0   →  and v0.16b, v0.16b, v1.16b
   ```

### 阶段2：实现核心组件（预计5-7天）

#### 2.1 S-box实现（最关键）

**目标**：实现`camellia_sbox_4blocks`宏

```asm
.macro camellia_sbox_4blocks
    // 输入：v0-v3 (4个128位块)

    // 1. 预变换（Camellia → AES域）
    adr x3, .Lcamellia_sbox1_lo
    ld1 {v20.16b}, [x3]
    // ... filter_8bit操作

    // 2. AES SubBytes（硬件加速）
    movi v31.16b, #0
    aese v0.16b, v31.16b
    aese v1.16b, v31.16b
    // ...

    // 3. 撤销ShiftRows
    adr x3, .Linv_shift_rows
    ld1 {v30.16b}, [x3]
    tbl v0.16b, {v0.16b}, v30.16b
    // ...

    // 4. 后变换（AES域 → Camellia域）
    // ... filter_8bit操作

    // 输出：v0-v3 (变换后的块)
.endm
```

**验证**：
```bash
# 编写测试验证S-box正确性
# 与C intrinsics版本对比输出
```

#### 2.2 转置和byte-slicing

**目标**：实现`byteslice_16x16b_fast`

```asm
.macro transpose_4x4_bytes a0, a1, a2, a3
    // 使用zip1/zip2实现转置
    zip1 v16.4s, \a0\().4s, \a1\().4s
    zip2 v17.4s, \a0\().4s, \a1\().4s
    zip1 v18.4s, \a2\().4s, \a3\().4s
    zip2 v19.4s, \a2\().4s, \a3\().4s

    zip1 \a0\().2d, v16.2d, v18.2d
    zip2 \a1\().2d, v16.2d, v18.2d
    zip1 \a2\().2d, v17.2d, v19.2d
    zip2 \a3\().2d, v17.2d, v19.2d
.endm
```

**验证**：
```bash
# 使用tests/test_02_transpose.c的逻辑
# 验证转置正确性
```

#### 2.3 P-function（XOR网络）

**目标**：实现Camellia的线性变换

```asm
// P-function: 大量XOR操作
eor v0.16b, v0.16b, v5.16b
eor v1.16b, v1.16b, v6.16b
eor v2.16b, v2.16b, v7.16b
// ... 继续26个XOR操作
```

### 阶段3：组装完整函数（预计3-5天）

#### 3.1 主加密函数

```asm
.global camellia_encrypt_16blks_simd128_aarch64_asm
.type camellia_encrypt_16blks_simd128_aarch64_asm, %function
camellia_encrypt_16blks_simd128_aarch64_asm:
    // 参数：
    // x0 = ctx pointer
    // x1 = output pointer
    // x2 = input pointer

    // 1. 保存寄存器
    stp x29, x30, [sp, #-16]!
    stp d8, d9, [sp, #-16]!
    // ... 保存v8-v15（callee-saved）

    // 2. 加载16个块
    ldp q0, q1, [x2, #0]
    ldp q2, q3, [x2, #32]
    // ... 加载v0-v15

    // 3. Byte-slicing转换
    byteslice_16x16b_fast

    // 4. 初始密钥白化
    ldr x3, [x0, #0]  // 加载key_table[0]
    dup v31.2d, x3
    eor v0.16b, v0.16b, v31.16b
    // ... 对v0-v15

    // 5. 加密轮次
    mov x4, #0  // 轮计数器
.Lenc_loop:
    // 6轮Feistel
    roundsm16 v0, v1, ..., x4

    // FL/FL^-1层
    cmp x4, #24
    b.ge .Lenc_done
    fls16 ...
    add x4, x4, #8
    b .Lenc_loop

.Lenc_done:
    // 6. 最终密钥白化
    // ...

    // 7. 反byte-slicing
    byteslice_16x16b_inverse

    // 8. 存储16个块
    stp q0, q1, [x1, #0]
    stp q2, q3, [x1, #32]
    // ... 存储v0-v15

    // 9. 恢复寄存器
    ldp d8, d9, [sp], #16
    ldp x29, x30, [sp], #16
    ret
.size camellia_encrypt_16blks_simd128_aarch64_asm, .-camellia_encrypt_16blks_simd128_aarch64_asm
```

### 阶段4：测试和优化（预计5-7天）

#### 4.1 正确性测试

```bash
# 与C intrinsics版本对比
./test_assembly_vs_intrinsics

# 测试所有密钥长度
./test_128bit_key
./test_192bit_key
./test_256bit_key

# RFC 3713测试向量
./test_official_vectors
```

#### 4.2 性能测试

```bash
# 性能基准
./benchmark_assembly

# 目标：
# C intrinsics: 613 MiB/s
# 汇编版本:    650-680 MiB/s (5-10%提升)
```

#### 4.3 微优化

- 指令重排减少流水线停顿
- 寄存器重命名减少依赖
- 预加载减少内存延迟
- 循环展开

---

## 📐 代码量估算

参考x86汇编版本的规模：

```
camellia_simd128_x86-64_aesni_avx.S:
├─ 总行数: 1639行
├─ S-box宏: ~200行
├─ 转置/byteslice: ~300行
├─ roundsm16宏: ~400行
├─ 主函数: ~300行
├─ FL层: ~200行
└─ 密钥调度: ~200行
```

**AArch64汇编预计**：1500-2000行

---

## ⚠️ 关键挑战

### 1. AArch64 vs x86的差异

| 特性 | x86-64 | AArch64 |
|------|--------|---------|
| SIMD寄存器数 | 16个xmm (SSE/AVX) | 32个v寄存器 |
| 寄存器宽度 | 128位/256位 | 128位 |
| 指令语法 | AT&T/Intel | ARM |
| AES指令 | AESNI | Crypto Extensions |
| Shuffle指令 | vpshufb | tbl/tbx |

**优势**：AArch64有32个寄存器，减少内存访问
**劣势**：没有256位向量，只能做16块并行

### 2. 指令延迟和吞吐量

需要查阅Graviton3的微架构手册：
- AESE延迟：?周期
- TBL延迟：?周期
- EOR延迟：?周期

优化指令顺序以隐藏延迟。

### 3. 寄存器压力

16个数据块 + 临时变量需要很多寄存器：
- v0-v15: 数据块
- v16-v23: 临时
- v24-v27: 轮密钥
- v28-v31: 常量

需要仔细管理寄存器使用。

---

## ✅ 成功标准

### 最低要求

- ✅ 通过所有RFC 3713测试向量
- ✅ 与C intrinsics版本输出完全一致
- ✅ 支持128/192/256位密钥
- ✅ 正确的16块并行处理

### 性能目标

- 🎯 达到或超过C intrinsics的613 MiB/s
- 🎯 理想：650-700 MiB/s（5-15%提升）
- 🎯 可接受：600-650 MiB/s（与C intrinsics相当）

### 代码质量

- ✅ 清晰的注释
- ✅ 模块化的宏定义
- ✅ 遵循AArch64 calling convention
- ✅ 正确保存/恢复callee-saved寄存器

---

## 🎯 与Iakov批评的对照

### 之前的问题

> "circa 400 lines vs the original Kivilinna's ~2000 lines"

**之前**：假的C实现，400行，循环调用标量
**现在**：真正的汇编实现，目标1500-2000行

### 现在的合理性

✅ **有明确目标**：对标x86汇编版本
✅ **真实工作**：手写汇编，不是循环调用
✅ **可衡量收益**：可能5-15%性能提升
✅ **学习价值**：深入理解SIMD优化

**BUT**：Iakov警告过现代编译器很好。如果汇编版本没有明显提升，不如直接用C intrinsics。

---

## 📅 时间线（保守估计）

| 阶段 | 任务 | 时间 |
|------|------|------|
| 1 | 研究和理解 | 3-5天 |
| 2 | 实现核心组件 | 5-7天 |
| 3 | 组装完整函数 | 3-5天 |
| 4 | 测试和优化 | 5-7天 |
| **总计** | | **16-24天** |

**诚实的工作量**：3-4周的全职工作。

---

## 🎓 学习路径

### 第1周：准备

1. 深入研究x86汇编版本
   ```bash
   vim camellia_simd128_x86-64_aesni_avx.S
   # 逐行分析结构
   ```

2. 学习AArch64汇编
   - ARM Architecture Reference Manual
   - NEON程序员指南
   - Crypto Extensions规范

3. 研究Graviton3微架构
   - 流水线结构
   - 指令延迟表
   - 优化指南

### 第2周：实现

1. 从简单开始：filter_8bit宏
2. 再做转置
3. 组装S-box
4. 实现单轮

### 第3周：完善

1. 完整的加密函数
2. 解密函数
3. 密钥调度（或调用C版本）

### 第4周：优化

1. 性能测试
2. 微优化
3. 文档化

---

## 🚀 下一步行动

### 立即行动（今天）

```bash
# 1. 研究x86参考实现
less camellia_simd128_x86-64_aesni_avx.S

# 2. 创建指令映射文档
vim x86_to_aarch64_mapping.md

# 3. 实现第一个简单宏
vim camellia_simd128_aarch64_neon_crypto.S
# 实现一个filter_8bit的简化版本
```

### 本周目标

- [ ] 理解x86汇编的完整结构
- [ ] 创建x86→AArch64指令映射表
- [ ] 实现并测试filter_8bit宏
- [ ] 实现并测试transpose宏

### 验收标准

每实现一个组件：
1. 编写单元测试
2. 与C intrinsics对比
3. 验证正确性
4. 记录性能

**不要一次性"完成"整个文件！**

---

## 💡 重要提醒

### ✅ DO

- 逐步实现，每步验证
- 参考x86汇编版本的结构
- 使用tests/目录中的测试方法
- 与C intrinsics版本对比
- 诚实记录性能结果

### ❌ DON'T

- 不要创建空壳wrapper（已经删除了）
- 不要硬编码性能数字
- 不要跳步实现
- 不要假设"应该能工作"
- 不要急于"完成"

---

## 📚 参考资源

### 代码参考

- ✅ `camellia_simd128_x86-64_aesni_avx.S` - x86汇编版本
- ✅ `camellia_simd128_with_aes_instruction_set.c` - C intrinsics版本
- ✅ `tests/test_*.c` - 验证方法

### 文档参考

- ✅ `ANALYSIS_KIVILINNA.md` - 实现原理
- ✅ `BYTE_SLICING_EXAMPLE.md` - Byte-slicing详解
- ✅ `tests/README.md` - 测试方法

### 外部资源

- ARM Architecture Reference Manual
- NEON Programmer's Guide
- AWS Graviton Technical Guide
- "Block Ciphers: Fast Implementations on x86-64 Architecture" 论文

---

**现在你有了一个清晰的、诚实的项目计划。这是真正的工作，需要3-4周时间。** 💪
