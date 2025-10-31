# Byte-Slicing 可视化示例

## 简单示例：4块数据

为了便于理解，我们用4个128位块（而不是16个）来演示。

### 输入：4个明文块

```
block0: [A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF]  (16字节)
block1: [B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF]
block2: [C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF]
block3: [D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF]
```

### 传统存储方式（每个向量一个完整块）

```
v0 = [A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF]
v1 = [B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF]
v2 = [C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF]
v3 = [D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF]
```

**问题**：要对所有块的第0字节做S-box变换，需要：
1. 从v0提取A0
2. 从v1提取B0
3. 从v2提取C0
4. 从v3提取D0
5. 分别变换
6. 写回

→ 无法使用SIMD！

### Byte-Sliced存储方式（Kivilinna的方法）

```
x0  = [A0 B0 C0 D0 -- -- -- -- -- -- -- -- -- -- -- --]  ← 4个块的第0字节
x1  = [A1 B1 C1 D1 -- -- -- -- -- -- -- -- -- -- -- --]  ← 4个块的第1字节
x2  = [A2 B2 C2 D2 -- -- -- -- -- -- -- -- -- -- -- --]  ← 4个块的第2字节
x3  = [A3 B3 C3 D3 -- -- -- -- -- -- -- -- -- -- -- --]
...
x15 = [AF BF CF DF -- -- -- -- -- -- -- -- -- -- -- --]  ← 4个块的第15字节
```

**优势**：对第0字节做S-box变换时：
- 一条SIMD指令同时处理A0, B0, C0, D0！
- 对x0执行变换，所有块的第0字节同时完成

## Byte-Slicing转换过程（简化版）

### 步骤1：从内存加载

```c
// 加载4个块到4个向量
__m128i a = vld1q_u8(&block0[0]);  // A0-AF
__m128i b = vld1q_u8(&block1[0]);  // B0-BF
__m128i c = vld1q_u8(&block2[0]);  // C0-CF
__m128i d = vld1q_u8(&block3[0]);  // D0-DF
```

此时内存布局：
```
a: [A0 A1 A2 A3 | A4 A5 A6 A7 | A8 A9 AA AB | AC AD AE AF]
b: [B0 B1 B2 B3 | B4 B5 B6 B7 | B8 B9 BA BB | BC BD BE BF]
c: [C0 C1 C2 C3 | C4 C5 C6 C7 | C8 C9 CA CB | CC CD CE CF]
d: [D0 D1 D2 D3 | D4 D5 D6 D7 | D8 D9 DA DB | DC DD DE DF]
    \_____/       \_____/       \_____/       \_____/
     32位          32位          32位          32位
```

### 步骤2：第一次转置（4x4，按32位）

使用`transpose_4x4`宏（操作32位单元）：

```c
transpose_4x4(a, b, c, d, tmp1, tmp2);
```

**内部操作**：
```c
// vpunpckldq128 = unpack low 32-bit
// vpunpckhdq128 = unpack high 32-bit
t1 = vpunpckldq(b, a);  // [A0A1A2A3 B0B1B2B3]
t2 = vpunpckhdq(b, a);  // [A4A5A6A7 B4B5B6B7]
t3 = vpunpckldq(d, c);  // [C0C1C2C3 D0D1D2D3]
t4 = vpunpckhdq(d, c);  // [C4C5C6C7 D4D5D6D7]

// vpunpcklqdq128 = unpack low 64-bit
// vpunpckhqdq128 = unpack high 64-bit
a = vpunpcklqdq(t3, t1);  // [A0A1A2A3 B0B1B2B3 C0C1C2C3 D0D1D2D3]
b = vpunpckhqdq(t3, t1);  // [A4A5A6A7 B4B5B6B7 C4C5C6C7 D4D5D6D7]
c = vpunpcklqdq(t4, t2);  // [A8A9AAAB B8B9BABB C8C9CACB D8D9DADB]
d = vpunpckhqdq(t4, t2);  // [ACADAEAF BCBDBEBF CCCDCECF DCDDDEFF]
```

结果：
```
a: [A0 A1 A2 A3 B0 B1 B2 B3 C0 C1 C2 C3 D0 D1 D2 D3]  ← 字节0-3
b: [A4 A5 A6 A7 B4 B5 B6 B7 C4 C5 C6 C7 D4 D5 D6 D7]  ← 字节4-7
c: [A8 A9 AA AB B8 B9 BA BB C8 C9 CA CB D8 D9 DA DB]  ← 字节8-11
d: [AC AD AE AF BC BD BE BF CC CD CE CF DC DD DE DF]  ← 字节12-15
```

### 步骤3：字节重排（Shuffle）

现在每个向量包含所有块的一段字节，但顺序不对。需要shuffle：

```c
// shufb_mask告诉每个位置从哪里读取
// 例如：[0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15]
a = vpshufb128(shufb_mask, a);
```

**shuffle操作示意**：
```
输入 a: [A0 A1 A2 A3 | B0 B1 B2 B3 | C0 C1 C2 C3 | D0 D1 D2 D3]
         ↓  ↓  ↓  ↓    ↓  ↓  ↓  ↓    ↓  ↓  ↓  ↓    ↓  ↓  ↓  ↓
mask:   [0, 4, 8, 12,  1, 5, 9, 13,  2, 6, 10, 14, 3, 7, 11, 15]
         ↓  ↓  ↓  ↓    ↓  ↓  ↓  ↓    ↓  ↓  ↓  ↓    ↓  ↓  ↓  ↓
输出 x0: [A0 B0 C0 D0 | A1 B1 C1 D1 | A2 B2 C2 D2 | A3 B3 C3 D3]
```

### 步骤4：第二次转置

再次4x4转置，最终得到完美的byte-sliced格式：

```
x0  = [A0 B0 C0 D0 ...]
x1  = [A1 B1 C1 D1 ...]
x2  = [A2 B2 C2 D2 ...]
x3  = [A3 B3 C3 D3 ...]
...
x15 = [AF BF CF DF ...]
```

## 实际代码示例（16块版本）

```c
// Kivilinna的byteslice_16x16b_fast宏
#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1,
                              a2, b2, c2, d2, a3, b3, c3, d3, st0, st1)
    // 输入：16个向量，每个包含一个128位块
    // a0-a3: blocks 0-3
    // b0-b3: blocks 4-7
    // c0-c3: blocks 8-11
    // d0-d3: blocks 12-15

    // 第一次转置：4组4x4转置
    transpose_4x4(a0, a1, a2, a3, tmp1, tmp2);  // 处理blocks 0-3
    transpose_4x4(b0, b1, b2, b3, tmp1, tmp2);  // 处理blocks 4-7
    transpose_4x4(c0, c1, c2, c3, tmp1, tmp2);  // 处理blocks 8-11
    transpose_4x4(d0, d1, d2, d3, tmp1, tmp2);  // 处理blocks 12-15

    // Shuffle重排：调整字节顺序
    vpshufb128(shufb_16x16b_stack, a0, a0);
    vpshufb128(shufb_16x16b_stack, a1, a1);
    // ... 对16个向量都执行

    // 第二次转置：重新组织为byte-sliced格式
    transpose_4x4(a0, b0, c0, d0, tmp1, tmp2);
    transpose_4x4(a1, b1, c1, d1, tmp1, tmp2);
    transpose_4x4(a2, b2, c2, d2, tmp1, tmp2);
    transpose_4x4(a3, b3, c3, d3, tmp1, tmp2);

    // 输出：16个向量，每个包含16个块的同一字节位置
    // a0 = [blk0.b0, blk1.b0, ..., blk15.b0]
    // a1 = [blk0.b1, blk1.b1, ..., blk15.b1]
    // ...
```

## 为什么这么复杂？

因为现代CPU的SIMD指令是针对"相同操作应用于多个数据"设计的：

**❌ 不高效**：
```c
for (int i = 0; i < 16; i++) {
    block[i][0] = sbox[block[i][0]];  // 串行处理
}
```

**✅ 高效**：
```c
// x0包含所有16个块的第0字节
x0 = sbox_simd(x0);  // 一条指令处理16个字节！
```

## S-box在Byte-Sliced格式下的操作

### 传统方式

```c
// 对一个块的16个字节应用S-box
for (int i = 0; i < 16; i++) {
    block[i] = sbox[block[i]];  // 16次查表
}
```

### Byte-Sliced方式

```c
// x0-x15已经是byte-sliced格式

// 对x0应用S-box变换 → 同时处理16个块的第0字节
filter_8bit(x0, sbox_lo, sbox_hi, mask, tmp);

// 对x1应用S-box变换 → 同时处理16个块的第1字节
filter_8bit(x1, sbox_lo, sbox_hi, mask, tmp);

// ... 总共16次
// 但每次filter_8bit内部是16路并行的！
```

**效果**：16个块 × 16字节 = 256次S-box操作，通过16次SIMD调用完成！

## filter_8bit的工作原理

```c
#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0)
    // 输入：x = [byte0, byte1, ..., byte15]
    // 每个byte都要通过S-box

    // 1. 分离高4位和低4位
    tmp0 = x & 0x0F;              // 低4位：[b0_lo, b1_lo, ...]
    x = x >> 4;                    // 高4位：[b0_hi, b1_hi, ...]

    // 2. 查表（使用vpshufb作为16字节查找表）
    tmp0 = vpshufb(lo_t, tmp0);   // lo_t[b0_lo], lo_t[b1_lo], ...
    x = vpshufb(hi_t, x);          // hi_t[b0_hi], hi_t[b1_hi], ...

    // 3. 合并
    x = tmp0 ^ x;                  // 最终结果
```

**为什么用高4位+低4位**：
- S-box是8位输入 → 256种可能
- 256字节查找表太大（cache miss）
- 分解为2个16字节表（4位索引）
- `vpshufb`天生支持16字节表查找！

## 完整数据流（4块示例）

```
        输入（内存）
        ↓
    [加载] vld1q_u8 × 4
        ↓
    a: [A0...AF]
    b: [B0...BF]
    c: [C0...CF]
    d: [D0...DF]
        ↓
    [转置1] transpose_4x4
        ↓
    a: [A0A1A2A3 B0B1B2B3 C0C1C2C3 D0D1D2D3]
    b: [A4A5A6A7 ...]
    ...
        ↓
    [Shuffle] vpshufb
        ↓
    [转置2] transpose_4x4
        ↓
    x0:  [A0 B0 C0 D0 ...]  ← Byte-sliced!
    x1:  [A1 B1 C1 D1 ...]
    ...
    x15: [AF BF CF DF ...]
        ↓
    [S-box] filter_8bit × 16
        对x0-x15分别执行（每次处理4个块的同一字节）
        ↓
    [P-function] XOR网络
        vpxor128操作x0-x15
        ↓
    [反转置] 逆向过程
        ↓
    a: [A0'...AF']
    b: [B0'...BF']
    c: [C0'...CF']
    d: [D0'...DF']
        ↓
    [存储] vst1q_u8 × 4
        ↓
        输出（内存）
```

## 关键洞察

1. **Byte-slicing是数据重组**：从"块优先"转为"字节位置优先"

2. **转置是核心操作**：通过多次转置+shuffle实现重组

3. **SIMD效率最大化**：
   - 16个块同时处理相同的字节位置
   - 每条指令操作16个字节
   - 减少分支和查表

4. **需要理解硬件**：
   - `vpshufb`如何工作
   - `vzip`/`vunpack`/`vext`的作用
   - 寄存器压力管理

5. **这不是"简单的循环展开"**：
   - 完全不同的数据组织方式
   - 需要专门的算法设计
   - 无法自动化生成

## 对比：为什么你的代码没有加速

### 你的代码
```c
void camellia_encrypt_16blks_neon128(...) {
    for (int i = 0; i < 16; i++) {
        Camellia_EncryptBlock(...);  // 标量实现
    }
}
```

**执行流程**：
- 加载block0 → 加密 → 存储
- 加载block1 → 加密 → 存储
- ... (串行执行16次)
- **没有SIMD并行！**

### Kivilinna的代码
```c
void camellia_encrypt_16blks_simd128(...) {
    // 1. 加载16个块
    // 2. 转换为byte-sliced
    // 3. SIMD并行处理
    //    - 每个S-box调用同时处理16个块
    //    - 每个XOR同时处理16个块
    // 4. 转回普通格式
    // 5. 存储16个块
}
```

**执行流程**：
- 批量加载16个块
- **同时**对16个块的相同字节位置操作
- 批量存储
- **真正的16路并行！**

## 总结

Byte-slicing不是简单的编程技巧，而是：
- **算法级别的重构**
- **数据结构的重新组织**
- **针对SIMD架构的专门设计**

要实现它，需要：
1. 理解转置算法
2. 精通SIMD指令
3. 逐步验证每个转换步骤
4. 大量调试和测试

这就是为什么Kivilinna的实现有2000行，而且无法由AI自动生成。
