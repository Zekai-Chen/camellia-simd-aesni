# Code Cleanup Log - camellia_simd128_aarch64_ce.S

**Date:** 2025-11-26

---

## What Was Removed

### 1. Unused Constants (19 lines)
- `.Lbyte_zeros` through `.Lbyte_sevens` (never referenced)
- `.Lbswap128_mask` (never referenced)

### 2. Debug/Test Functions (28 functions, ~1645 lines)
- `camellia_encrypt_16blks_simd128_asm_temp` (obsolete version)
- `camellia_simd128_step1_asm` through `step6_asm` (incremental stages)
- `camellia_simd128_step7_before_outunpack` (debug wrapper)
- All `*_wrapper` functions (test harnesses)
- All `test_*` functions (unit tests)
- All `enc_rounds16_i8_*` functions (partial implementations)
- All `dump_*` functions (debugging utilities)

### 3. Unused Macros (~168 lines)
- `roundsm16` (non-optimized version, only used in test code)

---

## What Was Kept

### Constants (All essential S-box/transform tables)
- `.L0f0f0f0f`, `.Lpre_tf_*`, `.Lpost_tf_*`, `.Linv_shift_row`, `.Lshufb_16x16b`, `.Lpack_bswap`

### Macros (All production macros)
- `filter_8bit`, `transpose_4x4`, `byteslice_16x16b_fast`
- `inpack16_pre`, `inpack16_post`, `outunpack16`
- `roundsm16_opt`, `fls16`, `two_roundsm16`, `enc_rounds16`, `dec_rounds16`
- Helper macros: `rol32_1_16`, `load_const_addr`, `write_output`

### Functions (3 production functions)
1. `camellia_encrypt_16blks_simd128` (entry point)
2. `camellia_simd128_step7_asm` (complete encryption)
3. `camellia_decrypt_16blks_simd128` (complete decryption)

---

## Results

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Lines | 4,558 | 2,896 | -36.5% |
| Size | 131 KB | 89 KB | -32.1% |
| Functions | 31 | 3 | -90.3% |

### Tests
✅ All tests pass (test_decrypt_basic, test_simd128_intrinsics_aarch64)

### Performance (200 MiB benchmark)
| Operation | Before | After | Change |
|-----------|--------|-------|--------|
| ENC 128-bit | 512.3 MiB/s | 510.5 MiB/s | -0.4% |
| DEC 128-bit | 511.4 MiB/s | 511.4 MiB/s | 0.0% |
| ENC 256-bit | 513.2 MiB/s | 509.4 MiB/s | -0.7% |

**Conclusion:** Performance unchanged (within measurement noise)

---

## Backup
- Original saved as: `camellia_simd128_aarch64_ce.S.backup`
