# AArch64 Assembly Implementation Progress

**项目**: Camellia SIMD128 AArch64 汇编实现
**开始日期**: 2025-10-27
**当前状态**: Phase 1 完成，开始 Phase 2

---

## 📊 整体进度

### Phase 1: 研究和准备 ✅ 完成
- [x] 删除假的 C 实现文件
- [x] 明确汇编实现的目标和范围
- [x] 深入研究 x86 汇编参考实现
- [x] 复制所有常量表到 AArch64
- [x] 创建实现范围文档和指令映射

### Phase 2: 核心组件实现 🔄 进行中
- [x] 实现 `filter_8bit` 宏 ✅ **已完成并测试通过**
- [ ] 实现转置宏 (transpose_4x4)
- [ ] 实现 byteslice 转换
- [ ] 实现 roundsm16 宏
- [ ] 实现 FL/FL^-1 层

### Phase 3: 完整函数组装 ⏳ 待开始
- [ ] 实现主加密循环
- [ ] 实现输入/输出处理
- [ ] 实现解密函数

### Phase 4: 测试和优化 ⏳ 待开始
- [ ] 单元测试
- [ ] 集成测试
- [ ] 性能测试

---

## ✅ 已完成的工作

### 1. 项目清理 (2025-10-27)
**任务**: 删除假的 C 实现文件

**完成内容**:
- 删除了 `camellia_aarch64_neon.c` (413 行假实现)
- 删除了 `camellia_aarch64_neon.h`
- 这些文件包含循环调用标量实现的假并行代码

**原因**: 这些文件导致了 Iakov 的批评，因为它们声称是 SIMD 实现但实际上只是 16 次串行调用。

---

### 2. 明确实现范围 (2025-10-27)
**任务**: 确定汇编实现的目标和范围

**创建的文档**:
- `ASSEMBLY_IMPLEMENTATION_SCOPE.md` - 详细的实现范围文档
  - 定义了所有需要实现的函数
  - x86 → AArch64 指令映射表
  - 寄存器分配策略
  - 预计代码量: 1500-1800 行
  - 性能目标: 650-700 MiB/s

**关键决策**:
- 目标是实现完整的汇编版本，类似 x86 的 1834 行实现
- 不是改进 C intrinsics，而是创建手写汇编
- 利用 AArch64 的 32 个寄存器优势

---

### 3. 研究 x86 参考实现 (2025-10-27)
**任务**: 分析 x86 汇编结构

**分析内容**:
- 读取了 `camellia_simd128_x86-64_aesni_avx.S` (1834 行)
- 理解了核心宏的结构:
  - `filter_8bit` - S-box 查表 (7 行)
  - `roundsm16` - 完整 S+P 层 (~120 行)
  - `inpack16_post` - Byte-slicing 输入
  - `enc_rounds16` - 6 轮加密宏

**关键发现**:
- x86 使用 `vaesenclast` (SubBytes + ShiftRows，无 MixColumns)
- AArch64 的 `aese` 包含 MixColumns，需要额外处理
- x86 使用 `vpshufb`，对应 AArch64 的 `tbl`

---

### 4. 复制常量表 (2025-10-27)
**任务**: 将 x86 的常量表复制到 AArch64

**创建的文档**:
- `X86_TO_AARCH64_CONSTANTS.md` - 常量表映射文档

**复制的常量表**:
```
✅ .Lpre_tf_lo_s1 / .Lpre_tf_hi_s1  (S-box 1,2,3 预变换)
✅ .Lpre_tf_lo_s4 / .Lpre_tf_hi_s4  (S-box 4 预变换)
✅ .Lpost_tf_lo_s1 / .Lpost_tf_hi_s1 (S-box 1,4 后变换)
✅ .Lpost_tf_lo_s2 / .Lpost_tf_hi_s2 (S-box 2 后变换)
✅ .Lpost_tf_lo_s3 / .Lpost_tf_hi_s3 (S-box 3 后变换)
✅ .Linv_shift_rows (撤销 AES ShiftRows)
✅ .Lnibble_mask (0x0f nibble mask)
✅ .Lshufb_16x16b (byte-slicing shuffle)
✅ .Ltranspose_8x8_shuf (8x8 转置 shuffle)
✅ .Lbyte_ones 到 .Lbyte_sevens (密钥扩展)
```

**文件更新**:
- `camellia_simd128_aarch64_neon_crypto.S` 从 196 行更新到 373 行
- 添加了所有必需的常量表
- 添加了详细的注释说明每个表的用途

---

### 5. 实现 filter_8bit 宏 ✅ (2025-10-27)
**任务**: 实现核心的 8-bit S-box 查表宏

**实现位置**: `camellia_simd128_aarch64_neon_crypto.S:177-192`

**代码**:
```asm
.macro filter_8bit x, lo_t, hi_t, mask4bit, tmp0
    // Extract low nibbles (bits 0-3 of each byte)
    and     \tmp0\().16b, \x\().16b, \mask4bit\().16b

    // Extract high nibbles (bits 4-7 of each byte)
    ushr    \x\().16b, \x\().16b, #4

    // Table lookup for low nibbles (16 parallel lookups)
    tbl     \tmp0\().16b, {\lo_t\().16b}, \tmp0\().16b

    // Table lookup for high nibbles (16 parallel lookups)
    tbl     \x\().16b, {\hi_t\().16b}, \x\().16b

    // XOR the two lookup results together
    eor     \x\().16b, \tmp0\().16b, \x\().16b
.endm
```

**测试**:
- 创建了 `tests/test_asm_filter_8bit.c`
- 实现了测试包装函数 `test_filter_8bit_asm`
- 与 C intrinsics 版本对比验证
- ✅ **所有测试通过！输出完全匹配！**

**测试结果**:
```
Input    : 00011223 34455667 78899aab bccddeef
Expected : 00011223 34455667 78899aab bccddeef
Got      : 00011223 34455667 78899aab bccddeef
✅ filter_8bit assembly macro works correctly!
```

**关键技术**:
1. 使用 `and` 提取低 4 位
2. 使用 `ushr #4` 提取高 4 位
3. 使用 `tbl` 指令进行 16 字节并行查表
4. 使用 `eor` 合并结果

**性能特性**:
- 5 条指令 (and, ushr, tbl, tbl, eor)
- 全部在寄存器中操作，无内存访问
- 16 字节并行处理
- 预计延迟: ~5-7 个时钟周期 (Graviton3)

---

## 📈 统计数据

### 代码行数
| 文件 | 行数 | 变化 |
|------|------|------|
| `camellia_simd128_aarch64_neon_crypto.S` | 373 行 | +177 行 (从 196) |
| 删除的 `camellia_aarch64_neon.c` | 0 | -413 行 |
| 删除的 `camellia_aarch64_neon.h` | 0 | -~100 行 |

### 新增文档
- `ASSEMBLY_IMPLEMENTATION_SCOPE.md` - 实现范围 (450+ 行)
- `X86_TO_AARCH64_CONSTANTS.md` - 常量映射 (370+ 行)
- `ASSEMBLY_PROGRESS.md` - 本文档

### 测试覆盖
- ✅ `tests/test_asm_filter_8bit.c` - filter_8bit 宏测试 (通过)

---

## 🎯 下一步工作

### 立即任务: 实现转置宏
**目标**: 实现 `transpose_4x4` 宏，用于 byte-slicing

**参考**:
- x86 实现: 使用 `vpunpckldq`, `vpunpckhdq`, `vpunpcklqdq`, `vpunpckhqdq`
- AArch64 需要使用: `zip1`, `zip2`, `uzp1`, `uzp2`, `trn1`, `trn2`

**x86 代码** (参考):
```asm
#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
    vpunpckhdq x1, x0, t2; \
    vpunpckldq x1, x0, x0; \
    vpunpckldq x3, x2, t1; \
    vpunpckhdq x3, x2, x2; \
    vpunpckhqdq t1, x0, x1; \
    vpunpcklqdq t1, x0, x0; \
    vpunpckhqdq x2, t2, x3; \
    vpunpcklqdq x2, t2, x2;
```

**AArch64 映射**:
```asm
.macro transpose_4x4 x0, x1, x2, x3, t1, t2
    // 使用 zip/uzp 指令实现 32 位元素转置
    // TODO: 完成实现
.endm
```

**测试计划**:
1. 创建 `tests/test_asm_transpose.c`
2. 测试 4x4 矩阵转置正确性
3. 与 `tests/test_02_transpose.c` 的 C 版本对比

---

### 后续任务 (优先级顺序)

#### 1. Byte-slicing 转换 (预计 2-3 天)
- 实现 `byteslice_16x16b` 宏
- 将 16 个 128 位块转换为 byte-sliced 格式
- 需要多次调用 `transpose_4x4`

#### 2. roundsm16 宏 (预计 3-4 天)
- 实现完整的 S-box + P-function 层
- 这是最复杂的宏 (~250-300 行)
- 包含:
  - Inverse ShiftRows
  - Pre-filter (使用 filter_8bit)
  - AES SubBytes (使用 aese)
  - Post-filter (使用 filter_8bit)
  - P-function (XOR 网络)
  - 轮密钥加

#### 3. FL/FL^-1 层 (预计 1 天)
- 密钥依赖线性变换
- 相对简单，主要是 AND + rotate + XOR

#### 4. 主加密循环 (预计 2-3 天)
- 组装所有组件
- 实现 18/24 轮加密
- 处理输入/输出

---

## 🔍 技术笔记

### AArch64 vs x86 关键差异

#### 1. AES 指令
- **x86 `vaesenclast`**: SubBytes + ShiftRows (无 MixColumns)
- **AArch64 `aese`**: SubBytes + ShiftRows + MixColumns
- **影响**: AArch64 需要额外处理或调整 post-filter 表

#### 2. 寄存器数量
- **x86**: 16 个 XMM 寄存器 (需要栈存储 CD 状态)
- **AArch64**: 32 个 V 寄存器 (可以全部放寄存器！)
- **优势**: AArch64 可以避免栈操作，提升性能

#### 3. Shuffle 指令
- **x86 `vpshufb`**: 字节级 shuffle
- **AArch64 `tbl`**: 完全等价的字节级 shuffle
- **差异**: 语法不同，功能相同

#### 4. 转置指令
- **x86**: `vpunpckldq`/`vpunpckhdq` (32-bit unpack)
- **AArch64**: `zip1`/`zip2` (交错), `uzp1`/`uzp2` (解交错)
- **复杂度**: AArch64 可能需要更多指令完成相同转置

---

## 📚 参考资料链接

### 项目文档
- [ASSEMBLY_IMPLEMENTATION_SCOPE.md](./ASSEMBLY_IMPLEMENTATION_SCOPE.md) - 实现范围
- [X86_TO_AARCH64_CONSTANTS.md](./X86_TO_AARCH64_CONSTANTS.md) - 常量映射
- [ASSEMBLY_PROJECT_PLAN.md](./ASSEMBLY_PROJECT_PLAN.md) - 项目计划
- [FILE_EXPLANATION.md](./FILE_EXPLANATION.md) - 文件说明
- [ANALYSIS_KIVILINNA.md](./ANALYSIS_KIVILINNA.md) - Kivilinna 分析
- [BYTE_SLICING_EXAMPLE.md](./BYTE_SLICING_EXAMPLE.md) - Byte-slicing 示例

### 代码文件
- `camellia_simd128_aarch64_neon_crypto.S` - AArch64 汇编实现 (进行中)
- `camellia_simd128_x86-64_aesni_avx.S` - x86 参考实现
- `camellia_simd128_with_aes_instruction_set.c` - C intrinsics 参考

### 测试文件
- `tests/test_asm_filter_8bit.c` - filter_8bit 测试 ✅
- `tests/test_01_basic_macros.c` - 基础宏测试 ✅
- `tests/test_02_transpose.c` - 转置测试 ✅
- `tests/test_03_filter_8bit.c` - filter_8bit C 版本测试 ✅

---

## 💡 经验教训

### 1. 渐进式开发的重要性
从最小的宏 (`filter_8bit`) 开始，而不是试图一次实现整个加密函数，这个方法被证明是有效的：
- ✅ 可以快速验证每个组件
- ✅ 更容易调试
- ✅ 逐步建立信心
- ✅ 避免大规模返工

### 2. 测试驱动开发
先创建 C intrinsics 测试 (test_03_filter_8bit.c)，然后再实现汇编版本，这帮助我们：
- ✅ 理解算法
- ✅ 有明确的正确性标准
- ✅ 快速验证汇编实现

### 3. 文档先行
在写代码前先创建详细的实现范围文档 (ASSEMBLY_IMPLEMENTATION_SCOPE.md) 非常有帮助：
- ✅ 明确目标和范围
- ✅ 识别技术挑战
- ✅ 规划寄存器分配
- ✅ 提供持续参考

### 4. 诚实和透明
删除假的实现 (camellia_aarch64_neon.c) 并重新开始，虽然痛苦但是必要的：
- ✅ 避免继续基于错误基础构建
- ✅ 建立真正的理解
- ✅ 可以展示真实进度

---

## 🎯 成功标准 (提醒)

### 功能性
- [ ] 通过 RFC 3713 所有测试向量
- [ ] 与 C intrinsics 输出完全一致
- [ ] 加密/解密互为逆运算

### 性能
- [ ] 达到 650 MiB/s 以上 (Graviton3)
- [ ] 比 C intrinsics (613 MiB/s) 快 5-15%

### 代码质量
- [ ] 清晰的注释
- [ ] 合理的寄存器分配
- [ ] 最小化栈使用

---

## 📅 时间线

- **2025-10-27**:
  - ✅ 项目清理和范围明确
  - ✅ x86 参考研究
  - ✅ 常量表复制
  - ✅ filter_8bit 实现和测试通过

- **2025-10-28 (计划)**:
  - [ ] 实现转置宏
  - [ ] 开始 byteslice 转换

- **Week 1 目标 (2025-10-27 ~ 2025-11-01)**:
  - [ ] 完成所有基础宏 (filter_8bit, transpose, byteslice)
  - [ ] 开始实现 roundsm16

---

**最后更新**: 2025-10-27 19:45 UTC
**当前阶段**: Phase 2 - 核心组件实现
**下一个里程碑**: 实现并测试 transpose_4x4 宏

---

### 6. 实现 transpose_4x4 宏 ✅ (2025-10-27)
**任务**: 实现 4x4 矩阵转置宏（32位元素）

**实现位置**: `camellia_simd128_aarch64_neon_crypto.S:226-242`

**代码**:
```asm
.macro transpose_4x4 x0, x1, x2, x3, t1, t2
    // First level: interleave 32-bit elements
    zip2    \t2\().4s, \x0\().4s, \x1\().4s
    zip1    \x0\().4s, \x0\().4s, \x1\().4s
    zip1    \t1\().4s, \x2\().4s, \x3\().4s
    zip2    \x2\().4s, \x2\().4s, \x3\().4s

    // Second level: interleave 64-bit pairs
    zip2    \x1\().2d, \x0\().2d, \t1\().2d
    zip1    \x0\().2d, \x0\().2d, \t1\().2d
    zip2    \x3\().2d, \t2\().2d, \x2\().2d
    zip1    \x2\().2d, \t2\().2d, \x2\().2d
.endm
```

**测试**:
- 创建了 `tests/test_asm_transpose.c`
- 实现了测试包装函数 `test_transpose_4x4_asm`
- 测试了简单模式和顺序模式
- ✅ **所有测试通过！输出完全匹配！**

**测试结果**:
```
Test 1: Simple 4x4 Transpose
Input:
x0: [00000000] [11111111] [22222222] [33333333]
x1: [44444444] [55555555] [66666666] [77777777]
x2: [88888888] [99999999] [aaaaaaaa] [bbbbbbbb]
x3: [cccccccc] [dddddddd] [eeeeeeee] [ffffffff]

Output (transposed - columns become rows):
o0: [00000000] [44444444] [88888888] [cccccccc] ✓
o1: [11111111] [55555555] [99999999] [dddddddd] ✓
o2: [22222222] [66666666] [aaaaaaaa] [eeeeeeee] ✓
o3: [33333333] [77777777] [bbbbbbbb] [ffffffff] ✓

Test 2: Sequential Pattern - PASSED ✓
```

**关键技术**:
1. **两级转置算法**:
   - 第一级: 使用 `zip1/zip2` 在 `.4s` 模式交错 32 位元素
   - 第二级: 使用 `zip1/zip2` 在 `.2d` 模式交错 64 位对
2. **8 条指令**: 高效的实现
3. **寄存器操作**: 全部在寄存器中完成，无内存访问

**指令映射**:
- x86 `vpunpckldq` → AArch64 `zip1 v.4s`
- x86 `vpunpckhdq` → AArch64 `zip2 v.4s`
- x86 `vpunpcklqdq` → AArch64 `zip1 v.2d`
- x86 `vpunpckhqdq` → AArch64 `zip2 v.2d`

**文件更新**:
- `camellia_simd128_aarch64_neon_crypto.S`: 373 行 → 487 行 (+114)
- 新增测试包装函数: `test_transpose_4x4_asm` (30 行)

---

## 📊 更新的统计数据

### 代码行数
| 文件 | 当前行数 | 变化 |
|------|---------|------|
| `camellia_simd128_aarch64_neon_crypto.S` | 487 行 | +291 行 (从 196) |
| 已实现的宏 | 2 个 | filter_8bit, transpose_4x4 |

### 测试覆盖
- ✅ `tests/test_asm_filter_8bit.c` - filter_8bit 测试 (通过)
- ✅ `tests/test_asm_transpose.c` - transpose_4x4 测试 (通过)

---

## 🎯 当前状态

**Phase 2 进度**: 40% 完成 (2/5 核心组件)

已完成:
- ✅ filter_8bit (S-box 查表)
- ✅ transpose_4x4 (矩阵转置)

待完成:
- [ ] byteslice_16x16b (完整 byte-slicing)
- [ ] roundsm16 (S+P 层)
- [ ] FL/FL^-1 层

**下一个里程碑**: 实现 byteslice_16x16b 转换

---

**最后更新**: 2025-10-27 20:15 UTC
