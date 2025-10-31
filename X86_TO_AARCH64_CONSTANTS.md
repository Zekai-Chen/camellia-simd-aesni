# x86 到 AArch64 常量表映射

**目的**: 记录从 x86 汇编复制到 AArch64 汇编的所有常量表

**来源**: camellia_simd128_x86-64_aesni_avx.S
**目标**: camellia_simd128_aarch64_neon_crypto.S

---

## 📋 所有需要的常量表

### 1. S-box Pre-transform Tables (预变换表)

#### S-box 1, 2, 3 的预变换
```asm
.Lpre_tf_lo_s1:
    .byte 0x45, 0xe8, 0x40, 0xed, 0x2e, 0x83, 0x2b, 0x86
    .byte 0x4b, 0xe6, 0x4e, 0xe3, 0x20, 0x8d, 0x25, 0x88

.Lpre_tf_hi_s1:
    .byte 0x00, 0x51, 0xf1, 0xa0, 0x8a, 0xdb, 0x7b, 0x2a
    .byte 0x09, 0x58, 0xf8, 0xa9, 0x83, 0xd2, 0x72, 0x23
```

**说明**: 将 Camellia S-box 输入转换到 AES 域
**公式**: swap_bitendianness(isom_map_camellia_to_aes(camellia_f(swap_bitendianess(in))))

#### S-box 4 的预变换（带 rotate）
```asm
.Lpre_tf_lo_s4:
    .byte 0x45, 0x40, 0x2e, 0x2b, 0x4b, 0x4e, 0x20, 0x25
    .byte 0x14, 0x11, 0x7f, 0x7a, 0x1a, 0x1f, 0x71, 0x74

.Lpre_tf_hi_s4:
    .byte 0x00, 0xf1, 0x8a, 0x7b, 0x09, 0xf8, 0x83, 0x72
    .byte 0xad, 0x5c, 0x27, 0xd6, 0xa4, 0x55, 0x2e, 0xdf
```

**说明**: S-box 4 的输入需要先循环左移 1 位

---

### 2. S-box Post-transform Tables (后变换表)

#### S-box 1 和 4 的后变换
```asm
.Lpost_tf_lo_s1:
    .byte 0x3c, 0xcc, 0xcf, 0x3f, 0x32, 0xc2, 0xc1, 0x31
    .byte 0xdc, 0x2c, 0x2f, 0xdf, 0xd2, 0x22, 0x21, 0xd1

.Lpost_tf_hi_s1:
    .byte 0x00, 0xf9, 0x86, 0x7f, 0xd7, 0x2e, 0x51, 0xa8
    .byte 0xa4, 0x5d, 0x22, 0xdb, 0x73, 0x8a, 0xf5, 0x0c
```

**说明**: 将 AES SubBytes 输出转换回 Camellia 域
**公式**: swap_bitendianness(camellia_h(isom_map_aes_to_camellia(swap_bitendianness(aes_inverse_affine_transform(in)))))

#### S-box 2 的后变换（带 rotate left）
```asm
.Lpost_tf_lo_s2:
    .byte 0x78, 0x99, 0x9f, 0x7e, 0x64, 0x85, 0x83, 0x62
    .byte 0xb9, 0x58, 0x5e, 0xbf, 0xa5, 0x44, 0x42, 0xa3

.Lpost_tf_hi_s2:
    .byte 0x00, 0xf3, 0x0d, 0xfe, 0xaf, 0x5c, 0xa2, 0x51
    .byte 0x49, 0xba, 0x44, 0xb7, 0xe6, 0x15, 0xeb, 0x18
```

**说明**: 输出需要循环左移 1 位

#### S-box 3 的后变换（带 rotate right）
```asm
.Lpost_tf_lo_s3:
    .byte 0x1e, 0x66, 0xe7, 0x9f, 0x19, 0x61, 0xe0, 0x98
    .byte 0x6e, 0x16, 0x97, 0xef, 0x69, 0x11, 0x90, 0xe8

.Lpost_tf_hi_s3:
    .byte 0x00, 0xfc, 0x43, 0xbf, 0xeb, 0x17, 0xa8, 0x54
    .byte 0x52, 0xae, 0x11, 0xed, 0xb9, 0x45, 0xfa, 0x06
```

**说明**: 输出需要循环右移 1 位

---

### 3. Shuffle 和 Mask 常量

#### Inverse ShiftRows (撤销 AES ShiftRows)
```asm
.Linv_shift_row:
    .byte 0x00, 0x0d, 0x0a, 0x07, 0x04, 0x01, 0x0e, 0x0b
    .byte 0x08, 0x05, 0x02, 0x0f, 0x0c, 0x09, 0x06, 0x03
```

**说明**: AES ShiftRows 的逆变换，用于从 `vaesenclast`/`aese` 的输出中隔离 SubBytes

**AES ShiftRows** (标准):
```
00 01 02 03      00 01 02 03      (row 0: no shift)
04 05 06 07  →   05 06 07 04      (row 1: left 1)
08 09 0a 0b  →   0a 0b 08 09      (row 2: left 2)
0c 0d 0e 0f      0f 0c 0d 0e      (row 3: left 3)
```

**Inverse ShiftRows**:
```
00 01 02 03      00 01 02 03
04 05 06 07  →   07 04 05 06      (row 1: right 1 = left 3)
08 09 0a 0b  →   0a 0b 08 09      (row 2: right 2)
0c 0d 0e 0f      0d 0e 0f 0c      (row 3: right 3 = left 1)
```

**索引映射**:
- Position 0 → 0x00
- Position 1 → 0x0d (取位置 13 的字节)
- Position 2 → 0x0a (取位置 10 的字节)
- Position 3 → 0x07 (取位置 7 的字节)
- ... 等等

#### 4-bit Nibble Mask
```asm
.L0f0f0f0f:
    .long 0x0f0f0f0f
```

**AArch64 版本** (扩展为 128 位):
```asm
.Lnibble_mask:
    .byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
    .byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
```

**用途**: 与运算提取字节的低 4 位（nibble）

#### 8x8 Transpose Shuffle
```asm
.Ltranspose_8x8_shuf:
    .byte 0, 1, 4, 5, 2, 3, 6, 7, 8+0, 8+1, 8+4, 8+5, 8+2, 8+3, 8+6, 8+7
```

**说明**: 用于 byte-slicing 的 8x8 字节转置

---

### 4. 密钥扩展常量

#### Byte Position Extractors
```asm
.Lbyte_ones:
    .quad 1 * 0x0101010101010101
    .quad 1 * 0x0101010101010101

.Lbyte_twos:
    .quad 2 * 0x0101010101010101
    .quad 2 * 0x0101010101010101

.Lbyte_threes:
    .quad 3 * 0x0101010101010101
    .quad 3 * 0x0101010101010101

.Lbyte_fours:
    .quad 4 * 0x0101010101010101
    .quad 4 * 0x0101010101010101

.Lbyte_fives:
    .quad 5 * 0x0101010101010101
    .quad 5 * 0x0101010101010101

.Lbyte_sixs:
    .quad 6 * 0x0101010101010101
    .quad 6 * 0x0101010101010101

.Lbyte_sevens:
    .quad 7 * 0x0101010101010101
    .quad 7 * 0x0101010101010101
```

**说明**: 用于从 8 字节密钥材料中提取特定字节位置，配合 `vpshufb`/`tbl` 使用

**用法示例** (x86):
```asm
vmovq   (key_ptr), %xmm0           # 加载 8 字节密钥材料到低 64 位
vpshufb .Lbyte_ones, %xmm0, %xmm1  # 提取字节 1，复制到整个向量
```

结果：如果密钥材料是 `[k0, k1, k2, k3, k4, k5, k6, k7]`，
那么 `vpshufb .Lbyte_ones` 会产生 `[k1, k1, k1, k1, k1, k1, k1, k1, k1, k1, k1, k1, k1, k1, k1, k1]`

---

### 5. Byte-slicing Shuffle Pattern

```asm
#define SHUFB_BYTES(idx) \
    0 + (idx), 4 + (idx), 8 + (idx), 12 + (idx)

.Lshufb_16x16b:
    .byte SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3)
```

**展开后**:
```asm
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
```

**说明**: 用于 byte-slicing 转换，从 4 个 32 位字中提取对应字节

**示例**:
```
输入向量: [b0 b1 b2 b3 | b4 b5 b6 b7 | b8 b9 ba bb | bc bd be bf]
              ↑32-bit↑     ↑32-bit↑     ↑32-bit↑     ↑32-bit↑

使用 .Lshufb_16x16b shuffle:
输出向量: [b0 b4 b8 bc | b1 b5 b9 bd | b2 b6 ba be | b3 b7 bb bf]
           ↑4个字的    ↑4个字的    ↑4个字的    ↑4个字的
           第0字节      第1字节      第2字节      第3字节
```

---

## 🔄 AArch64 差异和调整

### 指令差异
1. **x86 `vpshufb`** → **AArch64 `tbl`**
   - 功能相同：字节级查表/shuffle
   - 语法不同：`tbl vd.16b, {vn.16b}, vm.16b`

2. **x86 `vaesenclast`** → **AArch64 `aese`**
   - x86: SubBytes + ShiftRows (无 MixColumns)
   - AArch64: SubBytes + ShiftRows + MixColumns
   - **需要额外处理 MixColumns！**

3. **Broadcast 常量**
   - x86: `vbroadcastss .L0f0f0f0f(%rip), %xmm0`
   - AArch64: 需要先 `ldr` 或使用 `dup` 指令

---

## ✅ 常量表复制检查清单

### 已经存在于 AArch64 .S 文件中的：
- [x] `.Lnibble_mask` (但可能需要扩展)
- [x] `.Linv_shift_rows` (部分实现)
- [x] `.Lcamellia_sbox1_lo` / `_hi` (但可能不完整)
- [x] `.Lcamellia_sbox2_lo` / `_hi` (但可能不完整)

### 需要添加的：
- [ ] `.Lpre_tf_lo_s1` / `_hi_s1`
- [ ] `.Lpre_tf_lo_s4` / `_hi_s4`
- [ ] `.Lpost_tf_lo_s1` / `_hi_s1`
- [ ] `.Lpost_tf_lo_s2` / `_hi_s2`
- [ ] `.Lpost_tf_lo_s3` / `_hi_s3`
- [ ] `.Lbyte_ones` 到 `.Lbyte_sevens`
- [ ] `.Lshufb_16x16b`
- [ ] `.Ltranspose_8x8_shuf`

---

## 📝 实施步骤

### Step 1: 更新现有常量（如果不匹配）
检查当前 AArch64 .S 文件中的常量是否与 x86 版本一致。

### Step 2: 添加缺失的预变换表
复制所有 `pre_tf` 表到 `.rodata` 段。

### Step 3: 添加缺失的后变换表
复制所有 `post_tf` 表到 `.rodata` 段。

### Step 4: 添加辅助常量
复制 `byte_ones` 到 `byte_sevens`，以及 shuffle 模式。

### Step 5: 验证对齐
确保所有常量表 16 字节对齐（`.align 4` 在 AArch64 = 16 字节对齐）。

---

## 🔍 验证方法

### 方法 1: 十六进制对比
```bash
# 提取 x86 常量
grep -A1 "\.Lpre_tf_lo_s1:" camellia_simd128_x86-64_aesni_avx.S

# 对比 AArch64 常量
grep -A1 "\.Lpre_tf_lo_s1:" camellia_simd128_aarch64_neon_crypto.S
```

### 方法 2: 编写测试程序
创建一个测试程序加载这些常量并与参考值对比。

---

**下一步**: 将所有这些常量复制到 `camellia_simd128_aarch64_neon_crypto.S` 文件中
