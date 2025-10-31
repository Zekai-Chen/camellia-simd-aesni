# Byteslice Register Usage Trace

## X86 C Implementation Analysis

Based on `camellia_simd128_x86-64_aesni_avx.S` lines 393-440.

### Input Parameters
- Data registers: a0-a3, b0-b3, c0-c3, d0-d3 (16 vectors)
- Temporary registers for transpose: provided by caller (we use d2,d3 or a0,a1 or b0,b1)
- Stack storage: st0, st1 (two 128-bit memory locations)

### Phase 1: First 4x4 Transposes

| Step | Operation | d2 | d3 | a0 | a1 | st0 | st1 | Notes |
|------|-----------|----|----|----|----|-----|-----|-------|
| 1 | `vmovdqu d2, st0` | ? | ? | orig | orig | **d2_orig** | ? | Save d2 to stack |
| 2 | `vmovdqu d3, st1` | ? | ? | orig | orig | d2_orig | **d3_orig** | Save d3 to stack |
| 3 | `transpose_4x4(a0,a1,a2,a3,d2,d3)` | tmp | tmp | **trans** | **trans** | d2_orig | d3_orig | d2,d3 corrupted |
| 4 | `transpose_4x4(b0,b1,b2,b3,d2,d3)` | tmp | tmp | trans | trans | d2_orig | d3_orig | d2,d3 still tmp |
| 5 | `vmovdqu st0, d2` | **d2_orig** | tmp | trans | trans | d2_orig | d3_orig | Restore d2 |
| 6 | `vmovdqu st1, d3` | d2_orig | **d3_orig** | trans | trans | d2_orig | d3_orig | Restore d3 |
| 7 | `vmovdqu a0, st0` | d2_orig | d3_orig | trans | trans | **a0_trans** | d3_orig | Save a0 to stack |
| 8 | `vmovdqu a1, st1` | d2_orig | d3_orig | trans | trans | a0_trans | **a1_trans** | Save a1 to stack |
| 9 | `transpose_4x4(c0,c1,c2,c3,a0,a1)` | d2_orig | d3_orig | tmp | tmp | a0_trans | a1_trans | a0,a1 corrupted |
| 10 | `transpose_4x4(d0,d1,d2,d3,a0,a1)` | **trans** | **trans** | tmp | tmp | a0_trans | a1_trans | d2,d3 transposed |

**After Phase 1:**
- a0-a3: transposed (a0,a1 are garbage now)
- b0-b3: transposed
- c0-c3: transposed
- d0-d3: transposed
- st0: holds original transposed a0
- st1: holds original transposed a1

### Phase 2: Shuffle All Vectors

| Step | Operation | a0 | a1 | d2 | d3 | st0 | st1 | Notes |
|------|-----------|----|----|----|----|-----|-----|-------|
| 11 | `vmovdqu shufb_16x16b, a0` | **mask** | tmp | trans | trans | a0_trans | a1_trans | Load shuffle mask |
| 12 | `vmovdqu st1, a1` | mask | tmp | trans | trans | a0_trans | **a1_trans_saved** | Save a1 (will shuffle later) |
| 13-26 | Shuffle 14 vectors | mask | tmp | **shuf** | **shuf** | a0_trans | a1_trans_saved | a2,a3,b0-b3,c0-c3,d0-d3,d2,d3 |
| 27 | `vmovdqu d3, st1` | mask | tmp | shuf | shuf | a0_trans | **d3_shuf** | Save shuffled d3 |
| 28 | `vmovdqu st0, d3` | mask | tmp | shuf | **a0_trans** | a0_trans | d3_shuf | Load saved a0_trans to d3 |
| 29 | `vpshufb a0, d3, a0` | **a0_shuf** | tmp | shuf | a0_trans | a0_trans | d3_shuf | Shuffle a0 |
| 30 | `vpshufb a0, a1, a1` | a0_shuf | **a1_shuf** | shuf | a0_trans | a0_trans | d3_shuf | Shuffle a1 |
| 31 | `vmovdqu d2, st0` | a0_shuf | a1_shuf | shuf | a0_trans | **d2_shuf** | d3_shuf | Save shuffled d2 |

**After Phase 2:**
- All 16 vectors shuffled: a0-a3, b0-b3, c0-c3, d0-d3
- st0: holds shuffled d2
- st1: holds shuffled d3

### Phase 3: Second 4x4 Transposes

| Step | Operation | d2 | d3 | b0 | b1 | st0 | st1 | Notes |
|------|-----------|----|----|----|----|-----|-----|-------|
| 32 | `transpose_4x4(a0,b0,c0,d0,d2,d3)` | tmp | tmp | tmp | shuf | d2_shuf | d3_shuf | d2,d3 corrupted, b0 partially corrupt |
| 33 | `transpose_4x4(a1,b1,c1,d1,d2,d3)` | tmp | tmp | tmp | tmp | d2_shuf | d3_shuf | d2,d3 still tmp, b1 corrupt |
| 34 | `vmovdqu st0, d2` | **d2_shuf** | tmp | tmp | tmp | d2_shuf | d3_shuf | Restore d2 |
| 35 | `vmovdqu st1, d3` | d2_shuf | **d3_shuf** | tmp | tmp | d2_shuf | d3_shuf | Restore d3 |
| 36 | `vmovdqu b0, st0` | d2_shuf | d3_shuf | trans | tmp | **b0_trans** | d3_shuf | Save b0 |
| 37 | `vmovdqu b1, st1` | d2_shuf | d3_shuf | trans | tmp | b0_trans | **b1_trans** | Save b1 |
| 38 | `transpose_4x4(a2,b2,c2,d2,b0,b1)` | **final** | d3_shuf | tmp | tmp | b0_trans | b1_trans | d2 gets final value |
| 39 | `transpose_4x4(a3,b3,c3,d3,b0,b1)` | final | **final** | tmp | tmp | b0_trans | b1_trans | d3 gets final value |
| 40 | `vmovdqu st0, b0` | final | final | **b0_trans** | tmp | b0_trans | b1_trans | Restore b0 |
| 41 | `vmovdqu st1, b1` | final | final | b0_trans | **b1_trans** | b0_trans | b1_trans | Restore b1 |

**After Phase 3:**
- All 16 vectors in final byte-sliced form: a0-a3, b0-b3, c0-c3, d0-d3

## Key Insights

1. **st0 and st1 lifecycle:**
   - Initially: save d2_orig, d3_orig
   - Mid-Phase1: save a0_trans, a1_trans
   - End-Phase2: save d2_shuf, d3_shuf
   - Mid-Phase3: save b0_trans, b1_trans

2. **Critical saves in Phase 2:**
   - Line 27: `vmovdqu d3, st1` - MUST save shuffled d3 before loading a0_trans
   - Line 31: `vmovdqu d2, st0` - MUST save shuffled d2 after shuffling a0

3. **Why Phase 3 needs those saves:**
   - First two transposes corrupt d2,d3 → restore from st0,st1
   - Those transposes also corrupt b0,b1 → save to st0,st1 before final transposes

## ARM Assembly Mapping

Need 6 temporary registers total:
- t0, t1: for saving b0,b1 in Phase 3
- t2, t3: for saving d2,d3 in Phase 3 (after shuffle, before first transposes)
- t4, t5: for saving d2,d3 in Phase 1, and saving a0,a1 in Phase 1

Actually, x86 uses STACK (st0, st1) instead of registers. We should do the same!
We can use stack memory for st0 and st1.
