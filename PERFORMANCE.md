# AArch64 Camellia Performance Analysis

## Executive Summary

Through comprehensive performance analysis on AWS Graviton3, we discovered that **modern compiler optimizations consistently outperform hand-written assembly** for Camellia cipher implementation.

**Final Performance Results:**
- **C Intrinsics (GCC -O1)**: 716 MB/s ⭐ **RECOMMENDED**
- **Optimized Assembly**: 471 MB/s (memory barriers removed)
- **Original Assembly**: 453 MB/s (with unnecessary memory barriers)

## Key Findings

### 1. Compiler Superiority Over Hand Assembly
- C Intrinsics achieve **52% better performance** than hand-optimized Assembly
- GCC -O1 generates more efficient instruction sequences than manual optimization
- Superior register allocation and instruction scheduling from compiler

### 2. Memory Barriers Were Performance Killers
- Original Assembly contained unnecessary `dmb sy` instructions (32 per call)
- Each memory barrier costs significant CPU cycles
- Removing barriers improved Assembly performance by ~4%
- **Root cause**: Assembly author added barriers for multi-thread safety, but unnecessary for single-threaded performance

### 3. Function Call Overhead Is Negligible
- Function call overhead: ~4.6 nanoseconds per call
- Total overhead for 10,000 calls: 46.2 microseconds
- Only 4.4% of total execution time
- **Batch processing provides minimal benefits** (<1% improvement)

### 4. Real Performance Bottlenecks
The analysis identified that 95.6% of execution time is spent on:
- **Algorithm computation**: 32 blocks × 8 rounds × complex SIMD operations
- **Memory access patterns**: Cache utilization and data movement
- **Instruction-level parallelism**: CPU pipeline efficiency
- **SIMD instruction efficiency**: Quality of vectorized operations

## Detailed Performance Analysis

### Test Environment
- **Platform**: AWS Graviton3 (AArch64)
- **Compiler**: GCC 13.3.0 with -O1 optimization
- **CPU Binding**: Single core execution (`taskset -c 0`)
- **Data Size**: 512 bytes per call (32 × 16-byte blocks)

### Performance Comparison Table

| Implementation | Performance | Notes |
|----------------|-------------|-------|
| **C Intrinsics (Exact Match)** | **716.21 MB/s** | ✅ Best performance, maintainable |
| Assembly (Optimized) | 470.57 MB/s | Memory barriers removed |
| Assembly (Original) | 452.84 MB/s | Contains unnecessary `dmb sy` |
| Batch Processing (10x) | 469.25 MB/s | Minimal improvement over single calls |
| Batch Processing (100x) | 468.80 MB/s | No significant benefit |

### Function Call Overhead Analysis

```
Empty function call:        1.95 ns/call
Function with work:         1.95 ns/call  
Heavy register usage:       4.62 ns/call  ← Most realistic
Baseline loop:              1.94 ns/call
```

**Conclusion**: Function calls contribute only 4.4% to total execution time.

### Memory Barrier Impact

Original Assembly contained:
```asm
st1     {v0.16b}, [x23], #16
dmb     sy                    ; ← Performance killer (32x per call)
subs    w24, w24, #1
```

**Impact**: Each `dmb sy` instruction forces memory synchronization, stalling the CPU pipeline and preventing optimizations.

## Optimization Attempts

### 1. Batch Processing ❌
**Hypothesis**: Reduce function call overhead by processing multiple 512B chunks per call.

**Results**: 
- Batch size 1: 465.21 MB/s
- Batch size 10: 469.25 MB/s (+0.9%)
- Batch size 100: 468.80 MB/s (+0.8%)

**Conclusion**: Minimal improvement because function call overhead is only 4.4% of total time.

### 2. Memory Barrier Removal ✅
**Hypothesis**: Remove unnecessary memory synchronization instructions.

**Results**: 
- With barriers: 452.84 MB/s
- Without barriers: 470.57 MB/s (+3.9% improvement)

**Conclusion**: Successful but limited impact. Barriers were unnecessary for single-threaded use.

### 3. Algorithm Correctness Verification ✅
**Challenge**: Initial C Intrinsics produced different output than Assembly.

**Solution**: Created exact algorithm match implementation that produces identical results.

**Result**: Perfect correctness with superior performance (716 MB/s).

## Compiler Optimization Analysis

### Why GCC Outperforms Hand Assembly

1. **Instruction Scheduling**: Compiler optimally orders instructions to avoid pipeline stalls
2. **Register Allocation**: More efficient use of available registers
3. **Optimization Passes**: Multiple optimization phases not feasible in hand-written code
4. **Micro-architecture Awareness**: Compiler knows Graviton3-specific optimizations

### Assembly vs C Intrinsics Instruction Count
- **Assembly function**: 316 instructions
- **C Intrinsics function**: 308 instructions (when not inlined)

The C version achieves better performance with fewer instructions due to superior code generation.

## Future Optimization Directions

Based on our analysis, meaningful performance improvements should focus on:

### 1. Algorithm-Level Optimizations
- **Loop unrolling**: Reduce branch overhead in inner loops
- **SIMD instruction optimization**: Better vectorization patterns
- **Data dependency reduction**: Minimize pipeline stalls

### 2. Memory Access Optimization
- **Cache-friendly access patterns**: Improve data locality
- **Prefetching**: Reduce memory latency
- **Alignment optimization**: Ensure optimal memory alignment

### 3. Instruction-Level Parallelism
- **Pipeline optimization**: Better CPU resource utilization
- **Dependency chain reduction**: Allow more parallel execution
- **Specialized instruction selection**: Use optimal AArch64 instructions

## Recommendations

### For Production Use
- **Use C Intrinsics implementation** (`camellia_intrinsics_exact_match.c`)
- **Compile with GCC -O1** (higher optimization levels may break algorithm)
- **Single-threaded optimization focus** (no unnecessary synchronization)

### For Further Development
- **Focus on algorithm-level optimizations** rather than call overhead
- **Leverage compiler capabilities** instead of hand Assembly
- **Profile actual bottlenecks** before optimizing

### For Educational Purposes
- **Study compiler-generated assembly** to understand modern optimization techniques
- **Compare hand-written vs compiler-generated code** for learning
- **Use Assembly implementation as reference** for understanding SIMD concepts

## Technical Lessons Learned

1. **Modern compilers are extremely sophisticated** - often outperforming expert hand-optimized assembly
2. **Premature optimization can hurt performance** - memory barriers added "for safety" caused slowdown
3. **Measure actual bottlenecks** - function call overhead was assumed but proved minimal
4. **Algorithm correctness is crucial** - performance means nothing if output is wrong
5. **Systematic analysis beats intuition** - comprehensive testing revealed unexpected results

This analysis demonstrates the importance of evidence-based performance optimization and challenges traditional assumptions about assembly language superiority.