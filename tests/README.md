# Component Tests - 组件测试

这个目录包含一系列**从简单到复杂**的单元测试，用于验证Camellia SIMD实现的各个组件。

## 🎯 测试策略

我们采用**自底向上**的测试方法：

1. **先测试最基础的操作** → 确保NEON intrinsics正确工作
2. **再测试中间组件** → 验证转置、查表等操作
3. **最后测试完整流程** → 组装所有部分

这样可以：
- ✅ 早期发现问题
- ✅ 隔离错误来源
- ✅ 建立信心，逐步推进

## 📋 测试列表

### Test 01: 基础宏定义 (`test_01_basic_macros.c`)

**测试目标**：验证AArch64 NEON intrinsics的基本操作

**包含测试**：
- ✓ 逻辑运算 (AND, OR, XOR, AND NOT)
- ✓ 位移操作 (左移、右移)
- ✓ 字节shuffle (`vpshufb`)
- ✓ filter_8bit的基础操作流程

**为什么重要**：
- 这些是所有后续操作的基础
- 确保宏定义与硬件行为一致
- 理解`vpshufb`作为查找表的用法

**预期输出**：
```
=== Test 1: Basic Logic Operations ===
✅ Logic operations passed!

=== Test 2: Shift Operations ===
✅ Shift operations passed!

=== Test 3: Byte Shuffle (vpshufb) ===
✅ Shuffle operations passed!

=== Test 4: filter_8bit Basics ===
✅ filter_8bit basics (identity transform) passed!
```

### Test 02: 转置操作 (`test_02_transpose.c`)

**测试目标**：验证`transpose_4x4`宏的正确性

**包含测试**：
- ✓ 简单4x4矩阵转置
- ✓ 数值验证（result[i][j] == original[j][i]）
- ✓ 字节级转置（模拟byteslice的第一步）

**为什么重要**：
- 转置是byte-slicing的核心操作
- 理解数据如何重新组织
- 这是最容易出错的地方之一

**预期输出**：
```
=== Test 1: Simple 4x4 Transpose ===
✅ Simple transpose completed

=== Test 2: Transpose Verification ===
✅ Transpose verification passed!
   Each result[i][j] == original[j][i]

=== Test 3: Byte-Level Transpose ===
✅ Byte-level transpose verification passed!
```

### Test 03: filter_8bit变换 (`test_03_filter_8bit.c`)

**测试目标**：验证S-box查找表变换的正确性

**包含测试**：
- ✓ 恒等变换（identity transform）
- ✓ 简单的自定义S-box
- ✓ 真实的Camellia预变换表
- ✓ 批量变换（16字节并行）

**为什么重要**：
- 这是S-box加速的核心机制
- 验证高4位+低4位的分离和查表
- 确保XOR合并正确

**预期输出**：
```
=== Test 1: Identity Transform ===
✅ Identity transform passed!

=== Test 2: Simple S-box Transform ===
✅ Simple S-box transform passed!

=== Test 3: Camellia Pre-Transform Tables ===
✅ Camellia pre-transform test completed

=== Test 4: Batch Transform ===
✅ Batch transform passed!
```

## 🔨 编译和运行

### 在AArch64机器上（本地编译）

```bash
cd tests
make
make test
```

### 在x86机器上（交叉编译）

```bash
cd tests
make  # 自动使用 aarch64-linux-gnu-gcc

# 复制到ARM机器
scp test_* user@arm-machine:~/

# 在ARM机器上运行
ssh user@arm-machine
./test_01_basic_macros
./test_02_transpose
./test_03_filter_8bit
```

### 单独运行某个测试

```bash
./test_01_basic_macros
```

## 📊 理解测试输出

每个测试会打印：

1. **输入数据**：以十六进制显示向量内容
2. **中间步骤**：显示转换过程
3. **输出数据**：最终结果
4. **验证结果**：✅ 或 ❌

### 示例输出解读

```
Input a: f0 f0 f0 f0 f0 f0 f0 f0  f0 f0 f0 f0 f0 f0 f0 f0
Input b: aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa
a AND b: a0 a0 a0 a0 a0 a0 a0 a0  a0 a0 a0 a0 a0 a0 a0 a0
        ^^
        |
        └─ 0xF0 & 0xAA = 0xA0 ✓
```

## 🐛 调试技巧

### 如果测试失败

1. **查看输入/输出**：
   - 打印的十六进制值是否符合预期？
   - 中间步骤在哪里出错？

2. **手动计算**：
   - 用纸笔计算预期结果
   - 对比实际输出

3. **简化测试**：
   - 修改输入为更简单的值（如全0或全F）
   - 逐字节验证

4. **添加更多打印**：
   ```c
   print_m128i_bytes("Debug point X", some_vector);
   ```

### 常见问题

**Q: 为什么使用-O0编译？**
A: 调试时不优化，便于理解执行流程。验证正确后可以用-O2/-O3。

**Q: 交叉编译的二进制无法运行？**
A: 使用`-static`标志，或确保目标机器有相同的库。

**Q: 如何验证硬件支持？**
A: 在ARM机器上：
```bash
cat /proc/cpuinfo | grep Features
# 应该看到: aes neon ...
```

## 🎓 学习路径

推荐按以下顺序学习：

1. **阅读** `ANALYSIS_KIVILINNA.md` - 理解整体架构
2. **阅读** `BYTE_SLICING_EXAMPLE.md` - 理解byte-slicing概念
3. **运行** `test_01_basic_macros` - 熟悉NEON操作
4. **修改** 测试代码 - 尝试不同的输入
5. **运行** `test_02_transpose` - 理解数据重组
6. **运行** `test_03_filter_8bit` - 理解S-box变换
7. **编写** 你自己的测试 - 验证理解

## ✅ 验收标准

在进入下一步（实现完整byte-slicing）之前，确保：

- [ ] 所有3个测试都通过
- [ ] 理解每个测试在验证什么
- [ ] 能够手动计算至少一个测试用例的预期结果
- [ ] 能够修改测试代码（添加新的测试用例）
- [ ] 理解为什么需要这些组件

## 🚀 下一步

完成这些测试后，你可以：

1. **实现完整的byteslice_16x16b_fast**
   - 组合多次transpose_4x4调用
   - 添加shuffle操作
   - 验证4个块的转换

2. **实现roundsm16宏**
   - 组合filter_8bit调用
   - 实现P-function (XOR网络)
   - 添加轮密钥

3. **实现完整的16块加密**
   - 集成inpack/outunpack
   - 实现enc_rounds16
   - 端到端测试

但**不要着急**！确保每个组件都完全理解和验证后再继续。
