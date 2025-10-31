# TODO: Implement roundsm16 C Wrapper

## Problem
The Assembly `roundsm16` macro (in camellia_simd128_aarch64_neon_crypto.S:449-698) produces incorrect output when all 16 input blocks are identical. It generates non-uniform bytes instead of preserving the SIMD property.

## Current Status
- Assembly code has been modified to call `roundsm16_wrapper_aarch64` instead of using the `roundsm16` macro
- All call sites prepared (lines 1461-1483, 1511-1521, 1540-1574, 1592-1626)
- Wrapper function signature defined but not implemented

## Implementation Options

### Option 1: Extract roundsm16 Macro (Recommended)
Create a standalone C file that:
1. Defines all necessary platform-specific macros (vpxor128, vpshufb128, etc.)
2. Defines all lookup tables (inv_shift_row, mask_0f, pre_tf_*, post_tf_*, etc.)
3. Implements roundsm16 as a proper C function

**Challenge**: The C implementation uses complex macro system with platform-specific definitions and stack-based constant loading.

### Option 2: Debug Assembly roundsm16 (Long-term Solution)
Fix the bug in the Assembly macro itself:
- Location: camellia_simd128_aarch64_neon_crypto.S:449-698
- Bug symptoms: Non-uniform output bytes for uniform input
- Likely issues to investigate:
  - Inverse shift rows mask application
  - Pre/post filter table lookups
  - P-function XOR network
  - Key byte broadcast mechanism

### Option 3: Use Complete C Implementation (Quick Workaround)
Create wrapper that calls the full C `camellia_encrypt_16blks_simd128` for just one round.
- Pros: Quick to implement, guaranteed correctness
- Cons: Very inefficient, requires complex state management

## Files Involved
- `camellia_simd128_aarch64_neon_crypto.S` - Assembly with TODO marks
- `roundsm16_wrapper.c` - Wrapper interface (placeholder)
- `camellia_simd128_with_aes_instruction_set.c` - C reference implementation (lines 359-496)

## Next Steps
1. Create `roundsm16_extracted.c` with standalone implementation
2. Or: Debug Assembly roundsm16 systematically
3. Compile and link wrapper
4. Verify encryption output matches C reference

## Testing
Once implemented, test with:
```bash
gcc -O2 -march=armv8-a+crypto -o test_round_by_round \
    test_round_by_round.c \
    roundsm16_extracted.c \
    camellia_simd128_aarch64_neon_crypto.S \
    debug_globals.S \
    camellia_simd128_with_aes_instruction_set.c \
    -DDEBUG_ASM
./test_round_by_round
```

Expected: Assembly output matches C output (`67673138549669730857065648eabe43`)
