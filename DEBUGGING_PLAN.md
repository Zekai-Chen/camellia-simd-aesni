# Roundsm16 Debugging Plan

## Current Status
- Assembly output (no FL): dea3b5dc...
- Assembly output (with FL): f85db7be...
- Expected (C output): 67673138...

## Problem Confirmed
Roundsm16 has systematic bug. FL is working but can't fix underlying issue.

## Next Steps

### 1. Create minimal roundsm16 test
Create test that:
- Loads known AB state
- Runs single roundsm16(AB->CD)
- Compares byte-by-byte with C reference

### 2. Check each phase of roundsm16
- Phase 1: Inverse ShiftRows
- Phase 2: Pre-filter (input_mask)
- Phase 3: SubBytes (AESE)
- Phase 4: Post-filter (output_mask)
- Phase 5: P-function (XORs)
- Phase 6: Key addition + CD XOR

### 3. Likely suspects
Based on output pattern, check:
- Key byte extraction order (already fixed but verify)
- P-function XOR sequence
- CD state XOR (Phase 6)
- Register aliasing issues

### 4. Quick verification
Most likely issue is in Phase 5 (P-function) or Phase 6 (Feistel XOR).
The XOR pattern determines output and current output is systematically wrong.

## Files to Check
- camellia_simd128_aarch64_neon_crypto.S: lines 393-660 (roundsm16 macro)
- Focus on lines 567-660 (P-function and Phase 6)
