# Byteslice Assembly Implementation TODO

## 当前状态 (2025-10-30 Updated)

### ✅ 已完成部分 (RESOLVED!)
1. **pack_bswap修复**：Pre-whitening key的字节序转换已修复，AB值100%正确 ✓
2. **transpose_4x4修复**：修正了C wrapper中错误的transpose宏实现 ✓
3. **block 15寄存器保存bug修复**：修正了Assembly中v15被v0覆盖的问题 ✓
4. **byteslice验证成功**：AB和CD值after byteslice均100%匹配C参考实现 ✓

### ✅ 解决方案
**使用正确的C byteslice wrapper (byteslice_wrapper.c)**

#### 发现的Bug及修复
1. **Bug #1: transpose_4x4宏实现错误**
   - 位置: `byteslice_wrapper.c` lines 13-45
   - 问题: 使用了错误的vzip操作顺序，导致transpose结果不正确
   - 修复: 严格按照C参考实现的vpunpck*指令顺序重写transpose宏
   - 验证: `test_wrapper_identical.c` 测试通过

2. **Bug #2: Assembly中block 15寄存器覆盖**
   - 位置: `camellia_simd128_aarch64_neon_crypto.S` lines 1283-1301
   - 问题: v0 (包含block 15) 在保存到v15之前被block 0覆盖
   - 修复: 先执行 `mov v15.16b, v0.16b`，再执行 `mov v0.16b, v16.16b`
   - 结果: 所有16个blocks正确加载到v0-v15

#### 旧问题描述 (已解决)
C代码的`byteslice_16x16b_fast`宏使用两个**内存地址参数**（st0, st1），在3个Phase之间多次读写：
- Phase 1: 保存原始d2,d3 → transpose破坏 → 恢复d2,d3 → 加载到a0,a1
- Phase 2: shuffle → 保存d3 → 加载原始d2 → shuffle → 覆盖d2
- Phase 3: transpose → 保存临时值 → 加载到b0,b1 → transpose → 保存最终b0,b1

#### Assembly实现问题
最初错误地使用寄存器t4,t5来模拟st0/st1，但C代码的st0/st1是**内存位置**，需要多次独立读写。

#### C代码参考位置
`camellia_simd128_with_aes_instruction_set.c` lines 707-754:
```c
#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, \
                              a3, b3, c3, d3, st0, st1)
```

#### 测试工具
已创建隔离测试工具用于逐步验证：
- `test_byteslice_isolated_compare.c` - 对比C vs Assembly输出
- `test_transpose_only.c` - 验证transpose_4x4正确性
- `byteslice_final.S` - 尝试使用st1/ld1指令的版本（未完成）

### 📋 修复步骤（待后续完成）

1. **理解内存语义**
   - 研究C代码中st0/st1的完整生命周期
   - 画出3个Phase的数据流图

2. **正确的Assembly实现**
   - 使用栈空间模拟st0/st1（[sp+0]和[sp+16]）
   - 使用st1/ld1指令进行向量内存访问
   - 严格按照C代码的顺序逐行翻译

3. **逐Phase验证**
   - Phase 1单独测试
   - Phase 2单独测试
   - Phase 3单独测试
   - 整体测试

4. **性能优化**（可选）
   - 研究是否可以避免部分内存读写
   - 考虑使用AArch64特有的指令优化

### 🔧 临时方案
**当前解决方案**：使用C实现的byteslice（`camellia_simd128_with_aes_instruction_set.c`）

修改位置：`camellia_simd128_aarch64_neon_crypto.S` line ~1277
- 注释掉Assembly byteslice_16x16b调用
- 改为调用C函数wrapper

### 📚 参考资料
- Kivilinna的原始x86实现
- AArch64 NEON指令手册：st1/ld1指令用法
- 已验证正确的transpose_4x4实现（lines 231-247）

### ⚠️ 注意事项
- **不要**在修复byteslice之前修改其他部分，避免引入新bug
- **保持**隔离测试工具，用于回归测试
- **记录**每次修改的逻辑，便于debug

---

**最后更新**：2025-10-30
**优先级**：中（功能可用，但需要优化）
**预计工作量**：4-6小时深入调试
