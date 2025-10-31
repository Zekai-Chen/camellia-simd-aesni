# Camellia AArch64 Assembly Implementation - Bug Tracking

## Session Summary (2025-10-28)

### ✅ FIXED BUGS

#### BUG #1: Phase 4 Byte Remapping Disabled
- **Location**: byteslice_16x16b macro, line ~427
- **Issue**: `.if 0` disabled Phase 4 remapping
- **Impact**: Byteslice produced sequential output instead of interleaved
- **Fix**: Changed to `.if 1` to re-enable
- **Status**: ✅ FIXED and VERIFIED

#### BUG #2: roundsm16 Key Loading Used `dup` Instead of Zero-Extension
- **Location**: roundsm16 macro, line ~604
- **Issue**: Used `dup v_temp.2d, x11` which duplicates 64-bit key to both halves
- **Expected**: Zero-extend (key in low 64 bits, zeros in high 64 bits) like C's `vmovq128`
- **Impact**: Byte extraction via `tbl` got wrong values from high 64 bits
- **Fix**: Changed to:
  ```asm
  ld1 {t0.d}[0], [key_ptr]   // Load 8 bytes to t0.d[0]
  ins t0.d[1], xzr           // Insert zero into t0.d[1]
  ```
- **Status**: ✅ FIXED

#### BUG #3: roundsm16 Phase 6 Overwrote t6 Register
- **Location**: roundsm16 macro, line ~692
- **Issue**: Used t6 to load byte_zeros mask, overwriting previously extracted byte 6
- **Impact**: Key byte 6 was corrupted, causing wrong XOR with x1
- **Fix**: Use v24 (outside t0-t7 range) instead:
  ```asm
  ld1 {v24.16b}, [x11]
  tbl \t0\().16b, {\t0\().16b}, v24.16b
  ```
- **Status**: ✅ FIXED

### 🔍 POTENTIAL BUGS TO INVESTIGATE

#### ISSUE #1: CD State Update After First roundsm16
- **Location**: Main encryption loop
- **C Code Behavior**:
  ```c
  roundsm16(x0-x7, ..., mem_cd, key);  // AB → CD_new (in x0-x7)
  vmovdqa128(x4, mem_cd[0]);           // Store CD_new to mem_cd
  vmovdqa128(x5, mem_cd[1]);           // (with reordering)
  ...
  roundsm16(x4-x7,x0-x3, ..., mem_ab, key2);  // CD → AB_new
  ```
- **Assembly Code**: Appears to discard first roundsm16 output with comment "// CD pointer (STILL has CD_old!)"
- **Impact**: If CD is not updated after first roundsm16, second roundsm16 uses wrong input
- **Status**: ⚠️ NEEDS VERIFICATION

#### ISSUE #2: Register Reordering in two_roundsm16
- **C Code**: Second roundsm16 uses reordered parameters: `(x4,x5,x6,x7,x0,x1,x2,x3, ...)`
- **Assembly Code**: Uses `roundsm16 v4, v5, v6, v7, v0, v1, v2, v3, ...`
- **Status**: ✅ APPEARS CORRECT (needs verification)

#### ISSUE #3: FL/FLINV Functions Not Yet Tested
- **Status**: ❓ NOT TESTED

#### ISSUE #4: De-byteslice (outunpack16) Not Yet Tested
- **Status**: ❓ NOT TESTED

### 📊 TEST RESULTS

| Component | Status | Details |
|-----------|--------|---------|
| Byteslice (forward) | ✅ 100% PASS | Verified with test_byteslice_only_asm |
| Prewhiten | ✅ 100% PASS | Verified with test_prewhiten_compare |
| roundsm16 (isolated) | ❓ NOT TESTED | Need component test |
| Full encryption | ❌ FAIL | All 256 bytes differ, but output changing with each fix |
| Performance | ❓ NOT TESTED | - |

### 🎯 NEXT STEPS

1. **HIGH PRIORITY**: Verify CD state update in main loop
   - Check if first roundsm16 output is stored to mem_cd
   - Trace through assembly code step by step

2. **HIGH PRIORITY**: Create isolated roundsm16 test
   - Given known AB and CD inputs
   - Compare C vs Assembly output
   - Verify all sub-phases (S-box, P-function, key application)

3. **MEDIUM PRIORITY**: Test FL/FLINV functions
   - These run between round groups

4. **MEDIUM PRIORITY**: Test de-byteslice
   - Verify parameter ordering matches C code

5. **LOW PRIORITY**: Full integration testing
   - Only after all components verified individually

### 📝 NOTES

- Each bug fix has changed the output, indicating fixes are having effect
- Need systematic component-by-component testing
- Assembly code is structurally similar to C but has subtle register management issues
