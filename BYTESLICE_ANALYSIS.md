# Camellia Byteslice Transformation - 深入分析

## 🎯 目标
完全理解C代码的byteslice机制，特别是forward和reverse的parameter ordering

## 📊 C代码的两种调用

### Forward Byteslice (Input → Byte-sliced)
位置：`inpack16_post` macro, line 777
```c
byteslice_16x16b_fast(x0, x1, x2, x3, x4, x5, x6, x7, 
                      y0, y1, y2, y3, y4, y5, y6, y7, 
                      mem_ab[0], mem_cd[0]);
```
**参数顺序**：Sequential (x0-x7, y0-y7)
- x0-x7 = AB blocks (input blocks 0-7)
- y0-y7 = CD blocks (input blocks 8-15)

### Reverse Byteslice (Byte-sliced → Output)
位置：`outunpack16` macro, line 800
```c
byteslice_16x16b_fast(y0, y4, x0, x4, 
                      y1, y5, x1, x5, 
                      y2, y6, x2, x6, 
                      y3, y7, x3, x7, 
                      stack_tmp0, stack_tmp1);
```
**参数顺序**：Interleaved!
- Pattern: y0,y4,x0,x4, y1,y5,x1,x5, y2,y6,x2,x6, y3,y7,x3,x7

其中：
- x0-x7 = AB (final cipher state)
- y0-y7 = CD (final cipher state)

## 🔬 byteslice_16x16b_fast 的工作原理

### 输入/输出约定
```
输入参数: a0,b0,c0,d0, a1,b1,c1,d1, a2,b2,c2,d2, a3,b3,c3,d3
            ^^^^^^^^^^^  ^^^^^^^^^^^  ^^^^^^^^^^^  ^^^^^^^^^^^
            Group 0      Group 1      Group 2      Group 3
```

### 操作步骤（从C代码line 702-749）
1. **4x4 Transpose** (lines 706-707): 处理a0-a3, b0-b3
2. **4x4 Transpose** (lines 713-714): 处理c0-c3, d0-d3
3. **vpshufb** (lines 718-732): 用shufb_16x16b重排所有vectors
4. **4x4 Transpose** (lines 738-739): 处理a0,b0,c0,d0 和 a1,b1,c1,d1
5. **4x4 Transpose** (lines 745-746): 处理a2,b2,c2,d2 和 a3,b3,c3,d3

### 关键insight：shufb_16x16b的作用
```c
static const __m128i shufb_16x16b =
  M128I_U32(SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3));

#define SHUFB_BYTES(idx) \
    (((0 + (idx)) << 0)  | ((4 + (idx)) << 8) | \
     ((8 + (idx)) << 16) | ((12 + (idx)) << 24))
```

展开后：
- SHUFB_BYTES(0) = 0x0c080400
- SHUFB_BYTES(1) = 0x0d090501
- SHUFB_BYTES(2) = 0x0e0a0602
- SHUFB_BYTES(3) = 0x0f0b0703

Pattern: 每4个字节重排：0,4,8,12 → 1,5,9,13 → 2,6,10,14 → 3,7,11,15

## 🔑 Forward vs Reverse的区别

### Forward Byteslice
**输入**: 16个normal blocks
**参数**: Sequential (x0-x7, y0-y7)
**输出**: 16个byte-sliced vectors

Mapping:
```
Input blocks:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
               └──────── x0-x7 ─────────┘  └────── y0-y7 ──────┘
After byteslice:
Output vectors: (complex transformation, need to trace through transposes)
```

### Reverse Byteslice  
**输入**: 16个byte-sliced vectors (AB + CD state)
**参数**: Interleaved (y0,y4,x0,x4, y1,y5,x1,x5, ...)
**输出**: 16个normal blocks

The interleaved parameter order is CRITICAL!

## 📋 Interleaved Order的含义

看reverse byteslice的参数：
```
Params: y0, y4, x0, x4, y1, y5, x1, x5, y2, y6, x2, x6, y3, y7, x3, x7
Maps to: a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, a3, b3, c3, d3
```

这意味着：
- a0 = y0 (CD[0])
- b0 = y4 (CD[4])
- c0 = x0 (AB[0])
- d0 = x4 (AB[4])
- a1 = y1 (CD[1])
- ... 以此类推

## 💡 关键发现

1. **C代码没有"Phase 4"**：注释line 749明确说 "does not adjust output bytes inside vectors"

2. **Interleaved order的作用**：
   - 不是为了interleave BYTES within vectors
   - 而是为了reorganize WHICH vectors go into WHICH positions

3. **Forward和Reverse都调用同一个macro**，但通过不同的parameter order实现不同的transformation

## 🎯 Assembly需要做的改变

1. ❌ 删除Phase 4 byte interleaving（已完成）
2. ✅ 保持C代码的exact transpose sequence
3. ✅ 理解interleaved parameter order如何影响最终block ordering
4. ✅ 可能需要在write_output时调整block order来补偿

## 下一步：创建详细的block mapping trace
