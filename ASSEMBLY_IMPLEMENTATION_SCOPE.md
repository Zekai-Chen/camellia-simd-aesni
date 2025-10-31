# AArch64 Assembly Implementation Scope

**创建日期**: 2025-10-27
**目标平台**: AWS Graviton3 (AArch64, NEON, Crypto Extensions)
**参考实现**: camellia_simd128_x86-64_aesni_avx.S (1834 lines, 48KB)

---

## 📋 项目目标

### 主要目标
实现完整的 AArch64 汇编版本的 Camellia SIMD128 加密算法，类似于现有的 x86-64 汇编实现。

### 具体要求
1. **正确性**: 通过所有 RFC 3713 测试向量
2. **性能目标**: 650-700 MiB/s (vs C intrinsics 的 613 MiB/s)
3. **代码量**: 预计 1500-1800 行汇编代码
4. **完整性**: 实现 16 块并行加密/解密

---

## 🎯 实现范围

### 必须实现的函数

#### 1. `camellia_encrypt_16blks_simd128_aarch64_asm`
```c
void camellia_encrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,  // x0
    void *out,                       // x1 (16 blocks = 256 bytes)
    const void *in                   // x2 (16 blocks = 256 bytes)
);
```

**功能**:
- 并行处理 16 个 128 位块
- 使用 byte-slicing 技术
- 使用 AES 指令加速 S-box
- 完整的 18/24 轮加密

**预计代码量**: ~800-1000 行

#### 2. `camellia_decrypt_16blks_simd128_aarch64_asm`
```c
void camellia_decrypt_16blks_simd128_aarch64_asm(
    struct camellia_simd_ctx *ctx,  // x0
    void *out,                       // x1
    const void *in                   // x2
);
```

**功能**:
- 与加密类似，但轮密钥顺序相反
- S-box 使用解密表

**预计代码量**: ~800-1000 行

#### 3. `camellia_keysetup_simd128` (可选，先使用 C 实现)
```c
int camellia_keysetup_simd128(
    struct camellia_simd_ctx *ctx,
    const void *key,
    unsigned int keylen
);
```

**当前状态**: 可以继续调用 C 实现
**未来优化**: 可以用汇编重写密钥调度（低优先级）

---

## 🔧 核心组件清单

### 1. 数据转换宏

#### `byteslice_16x16b` - Byte-slicing 转换
**x86 参考**: 见 C 实现的 `byteslice_16x16b_fast`
**功能**: 将 16 个 128 位块转换为 byte-sliced 格式
**输入**: v0-v15 (16 个普通块)
**输出**: v0-v15 (byte-sliced: 每个向量包含 16 个块的同一字节位置)
**预计代码量**: ~150-200 行

**关键步骤**:
1. 4x4 转置（32位元素）
2. Shuffle 重组
3. 字节级转置

#### `un_byteslice_16x16b` - 逆 byte-slicing
**功能**: 将 byte-sliced 格式转换回普通块
**预计代码量**: ~150-200 行

---

### 2. S-box 宏

#### `filter_8bit` - 8 位查表转换
**x86 参考**: Lines 30-37
```asm
.macro filter_8bit x, lo_t, hi_t, mask4bit, tmp0
    // 分离高低4位
    and     \tmp0, \x, \mask4bit      // 低4位
    ushr    \x, \x, #4                 // 高4位

    // 查表
    tbl     \tmp0, {\lo_t}, \tmp0     // 低位查表
    tbl     \x, {\hi_t}, \x            // 高位查表

    // XOR 合并
    eor     \x, \tmp0, \x
.endm
```
**预计代码量**: ~10 行

#### `roundsm16` - 完整的 S+P 层
**x86 参考**: Lines 51-172
**功能**:
1. Inverse ShiftRows（撤销 AES 的 ShiftRows）
2. Pre-filter（Camellia → AES 域转换）
3. AES SubBytes（使用 `aese` 指令）
4. Post-filter（AES → Camellia 域转换）
5. P-function（XOR 网络）
6. 添加轮密钥

**预计代码量**: ~250-300 行

**寄存器使用**:
- v0-v7: 当前块的 8 个字节（AB 状态）
- v8-v15: 剩余 8 个字节（CD 状态）
- v16-v23: 临时寄存器
- v24-v27: 轮密钥
- v28-v31: 常量（mask, 查表等）

---

### 3. FL/FL^-1 层

#### `fls` - FL 函数
**功能**: Camellia 的密钥依赖线性变换
**操作**: AND + rotate + XOR
**预计代码量**: ~30-40 行

#### `flsinv` - FL^-1 函数
**功能**: FL 的逆变换
**预计代码量**: ~30-40 行

---

### 4. 主加密循环

#### 轮结构（128 位密钥 = 18 轮）
```
初始化:
  - 加载输入块
  - byteslice_16x16b 转换
  - 预白化（XOR kw1, kw2）

6 轮加密:
  - roundsm16 × 6

FL 层:
  - fls(kl1)
  - flsinv(kl2)

6 轮加密:
  - roundsm16 × 6

FL 层:
  - fls(kl3)
  - flsinv(kl4)

6 轮加密:
  - roundsm16 × 6

后白化:
  - XOR kw3, kw4
  - un_byteslice_16x16b 转换
  - 存储输出块
```

**预计代码量**: ~200-300 行

---

## 📊 x86 → AArch64 指令映射

### 基础 SIMD 指令

| x86 指令 | AArch64 指令 | 功能 | 备注 |
|---------|-------------|------|------|
| `vpxor` | `eor` | 异或 | 完全等价 |
| `vpand` | `and` | 与 | 完全等价 |
| `vpor` | `orr` | 或 | 完全等价 |
| `vpandn` | `bic` | AND NOT | `bic vd, vn, vm` = vd = vn & ~vm |
| `vmovdqa` | `mov` | 向量移动 | AArch64 更简单 |
| `vmovdqu` | `ldr`/`str` | 未对齐加载/存储 | `ldr q0, [x0]` |

### 位移和 Shuffle

| x86 指令 | AArch64 指令 | 功能 | 备注 |
|---------|-------------|------|------|
| `vpsrld $4, x, x` | `ushr v0.16b, v0.16b, #4` | 字节右移 4 位 | 注意 `.16b` |
| `vpshufb` | `tbl` | 字节 shuffle/查表 | 核心指令！ |
| `vpunpckldq` | `zip1` + `uzp1` | 解包低 32 位 | 需要组合 |
| `vpunpckhdq` | `zip2` + `uzp2` | 解包高 32 位 | 需要组合 |
| `vpunpcklqdq` | `zip1` (64-bit) | 解包低 64 位 | `.2d` 模式 |
| `vpunpckhqdq` | `zip2` (64-bit) | 解包高 64 位 | `.2d` 模式 |

### AES 加速指令

| x86 指令 | AArch64 指令 | 功能 | 备注 |
|---------|-------------|------|------|
| `vaesenclast` | `aese` + `eor` | AES 最后一轮 | AArch64 分两步 |
| - | `aese v0.16b, v1.16b` | SubBytes + ShiftRows + MixColumns | 注意有 MixColumns！ |
| - | 需要 `eor` 撤销 MixColumns | - | 或使用 `aese v0, 0; eor v0, v1` |

**重要区别**:
- x86 的 `vaesenclast` = SubBytes + ShiftRows（无 MixColumns）
- AArch64 的 `aese` = SubBytes + ShiftRows + MixColumns
- **需要额外步骤撤销 MixColumns**（或者接受它，在 post-filter 中补偿）

---

## 🧮 寄存器分配策略

### x86-64 (16 个 XMM/YMM 寄存器)
```
%xmm0-%xmm7:  AB 状态（当前处理的 8 个字节）
%xmm8-%xmm15: CD 状态（另外 8 个字节）
```

### AArch64 (32 个 V 寄存器！)
```
v0-v15:   16 个 byte-sliced 块（完整的 AB+CD 状态）
v16-v23:  S-box 临时寄存器（pre/post transform）
v24-v27:  轮密钥缓存
v28:      Nibble mask (0x0f0f0f0f...)
v29:      Inverse ShiftRows mask
v30-v31:  额外临时寄存器
```

**优势**: AArch64 有更多寄存器，可以减少栈操作！

---

## 📚 需要的常量表

### 1. S-box 转换表（已有部分）
```
.Lpre_tf_lo_s1/.Lpre_tf_hi_s1    # S-box 1 预变换
.Lpre_tf_lo_s2/.Lpre_tf_hi_s2    # S-box 2 预变换
.Lpre_tf_lo_s3/.Lpre_tf_hi_s3    # S-box 3 预变换
.Lpre_tf_lo_s4/.Lpre_tf_hi_s4    # S-box 4 预变换

.Lpost_tf_lo_s1/.Lpost_tf_hi_s1  # S-box 1 后变换
.Lpost_tf_lo_s2/.Lpost_tf_hi_s2  # S-box 2 后变换
.Lpost_tf_lo_s3/.Lpost_tf_hi_s3  # S-box 3 后变换
.Lpost_tf_lo_s4/.Lpost_tf_hi_s4  # S-box 4 后变换
```

**来源**: 从 x86 实现中复制（值是平台无关的）

### 2. Shuffle 模式
```
.Linv_shift_rows    # 撤销 AES ShiftRows
.Lshufb_16x16b      # Byte-slicing shuffle 模式
```

### 3. 常量
```
.Lnibble_mask       # 0x0f0f0f0f... (分离高低 4 位)
.L0f0f0f0f          # 同上
.Lbyte_ones         # 用于密钥扩展（1,1,1,1...）
.Lbyte_twos         # (2,2,2,2...)
... (到 .Lbyte_sevens)
```

---

## ⚠️ 关键技术挑战

### 1. AES 指令差异
**问题**: x86 的 `vaesenclast` 只做 SubBytes+ShiftRows，而 AArch64 的 `aese` 还做 MixColumns

**解决方案**:
- **方案 A**: 在 `aese` 后添加 inverse MixColumns
- **方案 B**: 修改 post-filter 表来补偿 MixColumns 的影响
- **方案 C**: 使用 `aese v0, 0; eor v0, result` 技巧

**推荐**: 方案 B（修改查表，避免额外指令）

### 2. Byte-slicing 转置
**问题**: x86 的 `vpunpckldq/vpunpcklqdq` 在 AArch64 上需要用 `zip`/`uzp` 组合

**解决方案**: 仔细映射转置宏，参考 C intrinsics 实现

### 3. 栈使用
**问题**: x86 实现使用栈存储 CD 状态

**解决方案**: AArch64 有 32 个寄存器，可以全部放寄存器，避免栈操作！

---

## ✅ 成功标准

### 功能性
- [ ] 通过 RFC 3713 所有测试向量（128/192/256 位密钥）
- [ ] 与 C intrinsics 实现的输出完全一致
- [ ] 加密/解密互为逆运算（round-trip 测试）
- [ ] 处理边界情况（对齐/未对齐输入）

### 性能
- [ ] 达到 650 MiB/s 以上（Graviton3）
- [ ] 比 C intrinsics (613 MiB/s) 快 5-15%
- [ ] 无明显的性能回退

### 代码质量
- [ ] 清晰的注释（中英文双语）
- [ ] 合理的寄存器分配
- [ ] 最小化栈使用
- [ ] 符合 AArch64 calling convention

---

## 📅 实施计划（参考 ASSEMBLY_PROJECT_PLAN.md）

### Phase 1: 研究和映射（3-5 天）
- [x] 分析 x86 汇编结构
- [ ] 创建指令映射表（本文档）
- [ ] 理解 byte-slicing 算法
- [ ] 确定寄存器分配策略

### Phase 2: 实现核心组件（5-7 天）
- [ ] 实现 `filter_8bit` 宏
- [ ] 实现转置宏（transpose_4x4）
- [ ] 实现 `byteslice_16x16b`
- [ ] 实现 `roundsm16` 宏
- [ ] 实现 FL/FL^-1 层

### Phase 3: 组装完整函数（3-5 天）
- [ ] 实现主加密循环
- [ ] 添加输入/输出处理
- [ ] 实现解密函数
- [ ] 添加所有常量表

### Phase 4: 测试和优化（5-7 天）
- [ ] 单元测试（每个宏单独测试）
- [ ] 集成测试（完整加密/解密）
- [ ] 性能测试和 profiling
- [ ] 优化热点路径

---

## 🔍 下一步行动

### 立即开始
1. **复制常量表**: 从 x86 实现复制所有 pre/post transform 表到 AArch64 .S 文件
2. **实现 filter_8bit**: 这是最简单也是最核心的宏（~10 行）
3. **测试 filter_8bit**: 使用现有的 test_03_filter_8bit.c 验证

### 本周目标
- 完成 `filter_8bit` 实现和测试
- 开始实现转置宏
- 复制所有需要的常量表

---

## 📚 参考资料

### 代码文件
- `camellia_simd128_x86-64_aesni_avx.S` - x86 汇编参考（1834 行）
- `camellia_simd128_with_aes_instruction_set.c` - C intrinsics 参考（2164 行）
- `tests/test_0*.c` - 已验证的组件测试

### 文档
- `ANALYSIS_KIVILINNA.md` - Kivilinna 实现分析
- `BYTE_SLICING_EXAMPLE.md` - Byte-slicing 可视化
- `ASSEMBLY_PROJECT_PLAN.md` - 项目计划

### 技术规范
- ARM Architecture Reference Manual (ARMv8)
- ARM NEON Intrinsics Reference
- ARM Crypto Extensions specification
- RFC 3713 - Camellia cipher specification

---

## 💡 关键原则

### 诚实和透明
- ✅ 实现真正的汇编代码
- ✅ 不使用假的 wrapper（`b C_function`）
- ✅ 每个组件都真正实现和测试
- ✅ 真实的性能测试结果

### 渐进式开发
- ✅ 从最小的宏开始（filter_8bit）
- ✅ 逐步构建复杂组件
- ✅ 每一步都测试验证
- ✅ 不跳步，不假设

### 质量优先
- ✅ 正确性 > 性能
- ✅ 可读性 > 代码行数
- ✅ 测试驱动开发
- ✅ 清晰的注释和文档

---

**最后更新**: 2025-10-27
**状态**: ✅ 范围明确，准备开始实现
