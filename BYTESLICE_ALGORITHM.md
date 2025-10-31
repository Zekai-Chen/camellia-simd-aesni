# Byte-Slicing Algorithm 详解

**目的**: 理解 `byteslice_16x16b_fast` 宏的工作原理，为汇编实现做准备

**创建日期**: 2025-10-27
**参考**: camellia_simd128_with_aes_instruction_set.c:702-749

---

## 📋 问题定义

### 输入格式（普通块格式）
16 个 128 位块，每个块包含 16 字节：

```
块 0:  [b0_00 b0_01 b0_02 ... b0_0f]  (16 bytes)
块 1:  [b1_00 b1_01 b1_02 ... b1_0f]
块 2:  [b2_00 b2_01 b2_02 ... b2_0f]
...
块 15: [b15_00 b15_01 b15_02 ... b15_0f]
```

### 输出格式（Byte-sliced 格式）
16 个向量，每个向量包含所有块的同一字节位置：

```
向量 0:  [b0_00 b1_00 b2_00 ... b15_00]  <- 所有块的第 0 字节
向量 1:  [b0_01 b1_01 b2_01 ... b15_01]  <- 所有块的第 1 字节
向量 2:  [b0_02 b1_02 b2_02 ... b15_02]
...
向量 15: [b0_0f b1_0f b2_0f ... b15_0f]  <- 所有块的第 15 字节
```

### 为什么需要 Byte-slicing？
这种转换让 SIMD 指令可以并行处理所有 16 个块的同一字节位置，从而实现：
- 并行 S-box 变换（16 个块同时）
- 并行 P-function（16 个块同时）
- 极大提升吞吐量

---

## 🔧 算法结构

### 宏签名
```c
byteslice_16x16b_fast(
    a0, b0, c0, d0,    // Group 0 (4 blocks)
    a1, b1, c1, d1,    // Group 1 (4 blocks)
    a2, b2, c2, d2,    // Group 2 (4 blocks)
    a3, b3, c3, d3,    // Group 3 (4 blocks)
    st0, st1           // 2 temporary storage locations
)
```

**输入**: 16 个向量寄存器 (a0-d3)，每个包含 1 个块（16 字节）
**输出**: 同样 16 个寄存器，但内容已重新组织为 byte-sliced 格式
**临时**: st0, st1 用于保存中间结果（通常是栈或额外寄存器）

---

## 📐 算法步骤详解

### Phase 1: 第一级转置（按 4x4 组）

#### Step 1.1: 保存 d2, d3
```c
vmovdqa128(d2, st0);  // st0 = d2
vmovdqa128(d3, st1);  // st1 = d3
```
**原因**: 需要 d2, d3 作为 transpose_4x4 的临时寄存器

#### Step 1.2: 转置前两组
```c
transpose_4x4(a0, a1, a2, a3, d2, d3);  // 转置 Group A
transpose_4x4(b0, b1, b2, b3, d2, d3);  // 转置 Group B
```

**效果** (以 32 位为单位)：
```
Before:
a0: [A00 A01 A02 A03]    After:
a1: [A10 A11 A12 A13]    a0: [A00 A10 A20 A30]  <- Column 0
a2: [A20 A21 A22 A23]    a1: [A01 A11 A21 A31]  <- Column 1
a3: [A30 A31 A32 A33]    a2: [A02 A12 A22 A32]  <- Column 2
                         a3: [A03 A13 A23 A33]  <- Column 3
```

#### Step 1.3: 恢复和保存
```c
vmovdqa128(st0, d2);  // d2 = old_d2
vmovdqa128(st1, d3);  // d3 = old_d3

vmovdqa128(a0, st0);  // st0 = a0 (需要用 a0 做临时)
vmovdqa128(a1, st1);  // st1 = a1
```

#### Step 1.4: 转置后两组
```c
transpose_4x4(c0, c1, c2, c3, a0, a1);  // 转置 Group C
transpose_4x4(d0, d1, d2, d3, a0, a1);  // 转置 Group D
```

---

### Phase 2: Shuffle 重排（关键步骤！）

#### Step 2.1: 准备 shuffle 模式
```c
vmovdqa128(shufb_16x16b_stack, a0);  // a0 = shuffle pattern
vmovdqa128(st1, a1);                  // a1 = saved value
```

**shufb_16x16b 模式**:
```
.Lshufb_16x16b: .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
```

这个 shuffle 模式的作用是：从 4 个 32 位字中提取对应字节位置。

**Shuffle 示例**:
```
输入向量: [b0 b1 b2 b3 | b4 b5 b6 b7 | b8 b9 ba bb | bc bd be bf]
             ↑32-bit↑     ↑32-bit↑     ↑32-bit↑     ↑32-bit↑

Shuffle pattern: [0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15]

输出向量: [b0 b4 b8 bc | b1 b5 b9 bd | b2 b6 ba be | b3 b7 bb bf]
           ↑4 个字的    ↑4 个字的    ↑4 个字的    ↑4 个字的
           第 0 字节     第 1 字节     第 2 字节     第 3 字节
```

#### Step 2.2: 应用 shuffle 到所有向量（除 st0）
```c
vpshufb128(a0, a2, a2);  // 15 次 shuffle 调用
vpshufb128(a0, a3, a3);
vpshufb128(a0, b0, b0);
...
vpshufb128(a0, d3, d3);
```

**为什么**？
因为第一阶段的转置给了我们"列"，但每个向量内部的 4 个 32 位字还是按原来的顺序。Shuffle 重新排列这些字节，使得：
- 前 4 字节包含 4 个块的第 0 字节
- 第 5-8 字节包含 4 个块的第 1 字节
- 等等

#### Step 2.3: 处理保存的向量
```c
vmovdqa128(d3, st1);              // st1 = d3 (需要用)
vmovdqa128(st0, d3);              // d3 = saved value
vpshufb128(a0, d3, a0);           // shuffle a0
vmovdqa128(d2, st0);              // st0 = d2 (恢复)
```

---

### Phase 3: 第二级转置（最终组织）

#### Step 3.1: 再次转置前两组
```c
transpose_4x4(a0, b0, c0, d0, d2, d3);  // 转置新的 Group 0
transpose_4x4(a1, b1, c1, d1, d2, d3);  // 转置新的 Group 1
vmovdqa128(st0, d2);
vmovdqa128(st1, d3);
```

#### Step 3.2: 转置后两组
```c
vmovdqa128(b0, st0);
vmovdqa128(b1, st1);
transpose_4x4(a2, b2, c2, d2, b0, b1);  // 转置新的 Group 2
transpose_4x4(a3, b3, c3, d3, b0, b1);  // 转置新的 Group 3
vmovdqa128(st0, b0);
vmovdqa128(st1, b1);
```

**最终效果**：
每个向量现在包含所有 16 个块的同一字节位置！

---

## 🔍 为什么需要两次转置？

### 第一次转置
- 将每组 4 个块从"按行"变为"按列"
- 把相同位置的 32 位字聚集在一起

### Shuffle
- 在每个 32 位字内部重新排列字节
- 把 4 个块的同一字节位置聚集在一起（在 4 字节组内）

### 第二次转置
- 将重新排列后的数据再次转置
- 最终实现完整的 byte-slicing

---

## 📊 数据流示例（简化版，4 块）

### 输入（4 个块）
```
a0: [00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f]  Block 0
b0: [10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f]  Block 1
c0: [20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f]  Block 2
d0: [30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f]  Block 3
```

### 第一次转置后（按 32 位字）
```
a0: [00 01 02 03 | 10 11 12 13 | 20 21 22 23 | 30 31 32 33]  <- Word 0 from all
b0: [04 05 06 07 | 14 15 16 17 | 24 25 26 27 | 34 35 36 37]  <- Word 1 from all
c0: [08 09 0a 0b | 18 19 1a 1b | 28 29 2a 2b | 38 39 3a 3b]  <- Word 2 from all
d0: [0c 0d 0e 0f | 1c 1d 1e 1f | 2c 2d 2e 2f | 3c 3d 3e 3f]  <- Word 3 from all
```

### Shuffle 后（重新排列字节）
```
a0: [00 10 20 30 | 01 11 21 31 | 02 12 22 32 | 03 13 23 33]
b0: [04 14 24 34 | 05 15 25 35 | 06 16 26 36 | 07 17 27 37]
c0: [08 18 28 38 | 09 19 29 39 | 0a 1a 2a 3a | 0b 1b 2b 3b]
d0: [0c 1c 2c 3c | 0d 1d 2d 3d | 0e 1e 2e 3e | 0f 1f 2f 3f]
```

### 第二次转置后（最终结果）
```
a0: [00 10 20 30 04 14 24 34 08 18 28 38 0c 1c 2c 3c]  <- Byte 0 positions
b0: [01 11 21 31 05 15 25 35 09 19 29 39 0d 1d 2d 3d]  <- Byte 1 positions
c0: [02 12 22 32 06 16 26 36 0a 1a 2a 3a 0e 1e 2e 3e]  <- Byte 2 positions
d0: [03 13 23 33 07 17 27 37 0b 1b 2b 3b 0f 1f 2f 3f]  <- Byte 3 positions
```

**注意**: 这是简化的 4 块示例。实际是 16 块，逻辑相同但规模更大。

---

## 🎯 AArch64 实现要点

### 1. 寄存器分配
- **输入/输出**: v0-v15 (16 个块)
- **临时**: v16-v31 (AArch64 优势！有 32 个寄存器)
- **不需要栈**: 可以全部用寄存器完成

### 2. Shuffle 模式
已经在 `.Lshufb_16x16b` 常量中定义：
```asm
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
```

### 3. 指令对应
- `vmovdqa128` → `mov` (寄存器) 或 `ldr/str` (内存)
- `vpshufb128` → `tbl` (表查找)
- `transpose_4x4` → 我们已实现的宏 ✅

### 4. 优化机会
- AArch64 有 32 个寄存器，可以避免所有栈操作
- 可以保持更多中间结果在寄存器中
- 潜在的指令流水线优化

---

## 📝 实现计划

### Step 1: 创建简化版本
先实现 4 块的 byte-slicing，测试验证原理

### Step 2: 扩展到 16 块
完整实现，处理所有 16 个块

### Step 3: 测试验证
- 与 C intrinsics 版本对比
- 验证每个字节位置正确

### Step 4: 性能测试
- 测量执行时间
- 验证没有明显瓶颈

---

## ⚠️ 潜在陷阱

### 1. 寄存器冲突
第一阶段的转置会修改某些寄存器，必须先保存它们。

### 2. Shuffle 模式
必须正确加载 `.Lshufb_16x16b` 模式，这是算法的关键。

### 3. 数据对齐
确保所有向量操作的数据正确对齐。

### 4. 指令顺序
某些操作有依赖关系，不能随意重排。

---

## 🧪 测试策略

### Test 1: 恒等变换
输入 → byteslice → un-byteslice → 输出
应该得到原始输入

### Test 2: 已知模式
使用简单模式（如递增字节）验证每个位置

### Test 3: 与 C 对比
完全相同的输入，对比每个字节

---

**下一步**: 实现 `byteslice_16x16b` 宏到汇编文件中
