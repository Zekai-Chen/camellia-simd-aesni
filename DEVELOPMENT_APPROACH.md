# Development Approach for AArch64 Camellia Assembly Implementation

**Author:** Claude Code
**Date:** 2025-11-20
**Architecture:** AArch64 (ARMv8 + Crypto Extensions)

---

## Overview

This document describes the **incremental, test-driven approach** used to develop the AArch64 assembly implementation of Camellia SIMD128. The key principle was: **build small, test immediately, never proceed without verification**.

---

## Development Philosophy

### Core Principles

1. **Incremental Development**
   - Write smallest testable unit first
   - Verify correctness before adding complexity
   - Each stage builds on verified previous stages

2. **Test-Driven Assembly**
   - Every macro has a C reference for comparison
   - Test wrappers validate individual components
   - Integration tests verify combined functionality

3. **Conservative Progression**
   - Stage 1 → Stage 2 → ... → Stage 7 → Full encryption
   - Each stage stops at a logical checkpoint
   - Output buffers allow inspection of intermediate state

4. **Debugging Infrastructure First**
   - Test harnesses before production code
   - Wrapper functions to call assembly from C
   - Hex dump utilities for state inspection

---

## Development Stages

### Stage 0: Foundation (Macros and Building Blocks)

**Goal:** Establish core computational macros with verified correctness.

#### Step 0.1: S-box Emulation via AES-NI

**Implementation:**
```assembly
.macro filter_8bit in, lo_t, hi_t, mask4bit, tmp0
    // Split input into high/low nibbles
    ushr    \tmp0\().16b, \in\().16b, #4
    and     \in\().16b, \in\().16b, \mask4bit\().16b
    and     \tmp0\().16b, \tmp0\().16b, \mask4bit\().16b

    // Table lookup for affine transform
    tbl     \in\().16b, {\lo_t\().16b}, \in\().16b
    tbl     \tmp0\().16b, {\hi_t\().16b}, \tmp0\().16b
    eor     \in\().16b, \in\().16b, \tmp0\().16b
.endm

.macro roundsm16 x0, x1, x2, x3, x4, x5, x6, x7, \
                 t0, t1, t2, t3, t4, t5, t6, t7, \
                 mem_cd, key_addr
    // 1. Inverse ShiftRows (cancel AESE's ShiftRows)
    load_const_addr .Linv_shift_row, x10, \t4
    tbl \x0\().16b, {\x0\().16b}, \t4\().16b
    // ... (repeat for x1-x7)

    // 2. Pre-transform for Camellia S-boxes
    load_const_addr .Lpre_tf_lo_s1, x10, \t0
    load_const_addr .Lpre_tf_hi_s1, x10, \t1
    filter_8bit \x0, \t0, \t1, \t7, \t6  // S1 for byte 0
    // ... (repeat for other bytes)

    // 3. Apply AESE (SubBytes via AES S-box)
    eor \t4\().16b, \t4\().16b, \t4\().16b  // zero vector
    aese \x0\().16b, \t4\().16b
    // ... (repeat for x1-x7)

    // 4. Post-transform to Camellia S-box output
    load_const_addr .Lpost_tf_lo_s1, x10, \t0
    filter_8bit \x0, \t0, \t1, \t7, \t6
    // ... (repeat for all S-boxes)

    // 5. P-function (linear mixing)
    // ... XOR pattern for byte permutation

    // 6. XOR with CD state and round key
    // ... Final Feistel XOR
.endm
```

**Testing Strategy:**
1. Create C reference of `roundsm16` logic
2. Write assembly test wrapper: `test_roundsm16_asm()`
3. Compare outputs byte-by-byte
4. Verify with known test vectors

**Test Code:**
```c
void test_roundsm16_asm(uint8x16_t ab[8], uint8x16_t cd[8],
                        uint64_t key, uint8x16_t ab_out[8]);

// Test harness
uint8x16_t ab_c[8], ab_asm[8], cd[8];
// Initialize with test data
roundsm16_c(ab_c, cd, key);
test_roundsm16_asm(ab_asm, cd, key);
compare_output(ab_c, ab_asm);  // Must match!
```

**Key Discovery:**
- AESE includes ShiftRows, must apply inverse first
- Affine transform tables must exactly match C reference
- Byte ordering in byte-sliced representation is critical

---

#### Step 0.2: FL/FLINV Key-Dependent Functions

**Implementation:**
```assembly
.macro fls16 x0, x1, x2, x3, x4, x5, x6, x7, \
             mem_cd, addr_kl, addr_kr, \
             t0, t1, t2, t3, t4, t5, t6, t7
    // FL function: uses KL, KR keys
    // Key insight: Swap key assignments instead of inverse operations!

    // Load KL (64-bit) and broadcast to all 16 lanes
    ldr  x11, [\addr_kl]
    dup  v30.2d, x11

    // Extract individual key bits and broadcast
    dup  \t0\().16b, v30.b[0]
    // ... (repeat for 8 bytes)

    // FL operations: AND, rotate-left, XOR pattern
    and  \t4\().16b, \x2\().16b, \t0\().16b
    // ... (Camellia FL logic)
.endm
```

**Critical Insight - FL/FLINV Key Swapping:**
```c
// WRONG approach: Compute inverse operations
flinv(state, kl, kr) {
    // Complex inverse logic...
}

// CORRECT approach: Just swap the keys!
fl_encryption(state, kl, kr);    // Uses KL first, then KR
fl_decryption(state, kr, kl);    // Uses KR first, then KL (swapped!)
```

**Testing:**
1. Test FL alone: `test_fl_asm()`
2. Test FLINV by calling FL with swapped keys
3. Verify reversibility: `FL(FLINV(x)) == x`

---

#### Step 0.3: Byte-Slicing (inpack16/outunpack16)

**Purpose:** Convert between standard byte layout and byte-sliced SIMD representation.

**inpack16:** Input (16 blocks × 16 bytes) → Byte-sliced AB/CD (8+8 vectors)
```assembly
.macro inpack16_pre v0, v1, v2, v3, v4, v5, v6, v7, \
                    v8, v9, v10, v11, v12, v13, v14, v15, \
                    input_ptr, key, tmp, tmp_x
    // Load 16 blocks (256 bytes)
    ld1 {v0.16b-v3.16b}, [input_ptr], #64
    // ... (4 batches)

    // Pre-whitening: XOR with key[0]
    dup tmp.2d, key
    eor v0.16b, v0.16b, tmp.16b
    // ... (repeat for v0-v15)
.endm

.macro inpack16_post v0, v1, ..., mem_ab, mem_cd, ...
    // Transpose: 16 vectors of 16 bytes → 16 bytes each of 16 vectors
    // This creates byte-sliced representation where:
    //   - Each vector contains same byte position from all 16 blocks
    //   - Enables parallel SIMD operations

    // Use TRN1/TRN2 (transpose) and ZIP1/ZIP2 instructions
    trn1 tmp0.16b, v0.16b, v1.16b
    trn2 tmp1.16b, v0.16b, v1.16b
    // ... (complex transpose pattern)

    // Split into AB (first 8 bytes) and CD (last 8 bytes)
    st1 {ab0.16b-ab3.16b}, [mem_ab], #64
    st1 {cd0.16b-cd3.16b}, [mem_cd], #64
.endm
```

**outunpack16:** Byte-sliced AB/CD → Output (16 blocks × 16 bytes)
- Reverse transpose operation
- Apply post-whitening XOR with final round key
- Store in standard block format

**Testing:**
1. Test `inpack16(outunpack16(x)) == x` (round-trip)
2. Compare with C reference byte-slicing
3. Verify with random data

---

### Stage 1: First 6 Rounds (Simplest Encryption Path)

**Goal:** Test basic round structure without FL/FLINV complexity.

**Function:** `camellia_simd128_step1_asm`

**Flow:**
```
Input (256 bytes, 16 blocks)
    ↓
inpack16 (byteslice + pre-whitening with key[0])
    ↓
enc_rounds16(keys 2-7): 6 Feistel rounds
    ↓
Output: AB/CD buffers (intermediate state)
```

**Why start here?**
- No FL/FLINV layers (simpler control flow)
- Tests core Feistel structure
- Validates `roundsm16` macro in context
- Easy to debug: only 6 rounds to trace

**Testing:**
```c
void camellia_simd128_step1_asm(struct camellia_simd_ctx *ctx,
                                uint8_t ab_out[128],
                                uint8_t cd_out[128],
                                const uint8_t in[256]);

// Compare with C reference
camellia_encrypt_6rounds_c(ctx, ab_c, cd_c, in);
camellia_simd128_step1_asm(ctx, ab_asm, cd_asm, in);
compare(ab_c, ab_asm);  // Must match!
compare(cd_c, cd_asm);
```

**Common Bugs Found:**
- Key indexing off-by-one
- AB/CD buffer layout mismatch
- Register clobbering (forgot to save callee-saved regs)

---

### Stage 2: Adding First FL Layer

**Goal:** Introduce FL/FLINV while keeping complexity manageable.

**Function:** `camellia_simd128_step2_asm`

**Flow:**
```
Input → inpack16 → enc_rounds16(2-7) → FL1(keys 8,9) → Output AB/CD
```

**New Challenge:** FL function operates on CD state, requires loading from memory.

**Implementation Pattern:**
```assembly
// After enc_rounds16, AB is in v0-v7, CD in memory
stp q0, q1, [mem_ab, #0]    // Store AB
stp q2, q3, [mem_ab, #32]
// ... (store all AB)

// Load addresses of FL keys
add x25, ctx, #key_table + (8 * 8)   // key[8]
add x26, ctx, #key_table + (9 * 8)   // key[9]

// Call FL on CD (reads AB from memory)
ldp q0, q1, [mem_ab, #0]    // FL needs AB[0-7] for one step
fls16 v0, v1, ..., mem_cd, x25, x26, ...
```

**Testing:**
- Compare against C reference with FL
- Verify FL correctly modifies CD
- Check AB remains unchanged

**Bug:** Initial implementation didn't store AB before FL, causing random behavior!

---

### Stage 3: Completing First Round Triplet

**Goal:** enc_rounds16 → FL1 → enc_rounds16

**Function:** `camellia_simd128_step3_asm`

**Flow:**
```
inpack16 → enc_rounds16(2-7) → FL1(8,9) → enc_rounds16(10-15) → Output
```

**Key Learning:** After FL, must reload AB from memory before next `enc_rounds16`.

```assembly
// After FL1
stp q0-q7 back to mem_ab

// Before second enc_rounds16
ldp q0, q1, [mem_ab, #0]   // Reload AB
ldp q2, q3, [mem_ab, #32]
// ... then call enc_rounds16
```

**Testing:**
- Full 12-round path (6+6 with FL in middle)
- Critical for 128-bit keys (18 rounds total)

---

### Stage 4-6: Progressive Complexity

**Stage 4:** Add FL2 (second FL layer)
**Stage 5:** Add third enc_rounds16 + FL3
**Stage 6:** Add fourth enc_rounds16 (for 256-bit keys)

Each stage follows same pattern:
1. Implement incrementally
2. Test against C reference
3. Debug mismatches
4. Document discoveries

---

### Stage 7: Complete Encryption with Post-Whitening

**Goal:** Full encryption with final output.

**Function:** `camellia_simd128_step7_asm`

**Flow:**
```
Input (plaintext)
    ↓
inpack16 (pre-whitening with key[0])
    ↓
[Based on key length:]
  128-bit: enc_rounds16(2-7) → FL1 → enc_rounds16(10-15) → FL2 → enc_rounds16(18-23)
  192/256-bit: (add FL3 + enc_rounds16(26-31))
    ↓
outunpack16 (post-whitening with key[lastk+1], reverse byteslice)
    ↓
Output (ciphertext)
```

**Key Branching Logic:**
```assembly
ldr w22, [ctx, #key_length]
cmp w22, #16
bgt .L256bit_path    // 192/256-bit keys

.L128bit_path:
    // 18 rounds (3 × enc_rounds16)
    mov w25, #24     // lastk for post-whitening
    b .Loutunpack

.L256bit_path:
    // 24 rounds (4 × enc_rounds16)
    mov w25, #32     // lastk for post-whitening
    // Fall through to .Loutunpack
```

**Testing:**
```c
// Compare against C reference
camellia_encrypt_16blks_simd128_c(ctx, cipher_c, plain);
camellia_encrypt_16blks_simd128_asm(ctx, cipher_asm, plain);
compare(cipher_c, cipher_asm);

// Known test vectors (NIST, RFC)
test_vector_128bit();
test_vector_192bit();
test_vector_256bit();
```

---

### Stage 8: Decryption

**Goal:** Implement decryption using **same round function**.

**Key Insight:** Camellia decryption = encryption with **reversed key schedule**.

**Implementation Strategy:**
```assembly
// Encryption: keys in forward order
enc_rounds16(..., key_index=0)   // Uses keys 2,3,4,5,6,7
enc_rounds16(..., key_index=8)   // Uses keys 10,11,12,13,14,15

// Decryption: keys in reverse order
dec_rounds16(..., key_index=22)  // Uses keys 24,23,22,21,20,19
dec_rounds16(..., key_index=14)  // Uses keys 16,15,14,13,12,11

// FL/FLINV: swap key order
// Encryption: FL(state, key[8], key[9])
// Decryption: FL(state, key[9], key[8])  // Swapped!
```

**Macro for Decryption Rounds:**
```assembly
.macro dec_rounds16 x0, x1, ..., mem_ab, mem_cd, i
    // Direction = -1 (decrement key index)
    two_roundsm16 ..., i+4, dir=-1, store_ab=0
    two_roundsm16 ..., i+2, dir=-1, store_ab=0
    two_roundsm16 ..., i+0, dir=-1, store_ab=0
.endm
```

**FL Key Swapping for Decryption:**
```assembly
// Encryption
add x25, ctx, #key_table + (8*8)  // key[8] = kl
add x26, ctx, #key_table + (9*8)  // key[9] = kr
fls16 ..., x25, x26               // FL(kl, kr)

// Decryption
add x25, ctx, #key_table + (9*8)  // key[9] = kr
add x26, ctx, #key_table + (8*8)  // key[8] = kl
fls16 ..., x25, x26               // FL(kr, kl) - swapped!
```

**Testing:**
```c
// Encrypt-decrypt round trip
camellia_encrypt_16blks_simd128(ctx, cipher, plain);
camellia_decrypt_16blks_simd128(ctx, decrypted, cipher);
compare(plain, decrypted);  // Must match!
```

---

## Testing Infrastructure

### Unit Tests

**test_roundsm16_asm():** Test single round function
```c
void test_roundsm16(void) {
    uint8x16_t ab_c[8], ab_asm[8], cd[8];
    init_test_data(ab_c, cd);

    roundsm16_c(ab_c, cd, key);
    roundsm16_asm_wrapper(ab_asm, cd, key);

    assert(memcmp(ab_c, ab_asm, 128) == 0);
}
```

**test_fls16_asm():** Test FL/FLINV
```c
void test_fls16(void) {
    uint8x16_t ab[8], cd[8], cd_backup[8];
    memcpy(cd_backup, cd, 128);

    fls16_asm(ab, cd, kl, kr);
    fls16_asm(ab, cd, kr, kl);  // Inverse with swapped keys

    assert(memcmp(cd, cd_backup, 128) == 0);  // Should be restored
}
```

**test_byteslice():** Test inpack16/outunpack16 round-trip
```c
void test_byteslice(void) {
    uint8_t plain[256], recovered[256];
    uint8_t ab[128], cd[128];

    inpack16_asm(ab, cd, plain, key);
    outunpack16_asm(recovered, ab, cd, key);

    assert(memcmp(plain, recovered, 256) == 0);
}
```

### Integration Tests

**test_decrypt_basic.c:** Main test harness
```c
int main() {
    test_128bit_key();
    test_192bit_key();
    test_256bit_key();

    printf("All tests passed!\n");
    return 0;
}

void test_128bit_key() {
    // Test encrypt-decrypt round trip
    // Test against C reference
    // Test known vectors
}
```

---

## Performance Optimization Journey

### Phase 0: Establish Baseline

**Initial Performance:**
- ASM: 513.8 MiB/s
- C Reference: 575.1 MiB/s
- **Gap:** 12% slower than C

### Phase 1: Constant Loading Optimization (FAILED)

**Attempt 1:** Function-level constant loading
- Load 6 constants into v24-v29 at function entry
- Use throughout all rounds
- **Result:** -0.8% regression (513.8 → 509.6 MiB/s)

**Attempt 2:** Remove MOV overhead
- Use v24-v29 directly without copying
- **Result:** -0.9% regression (same as Attempt 1)

**Root Cause:** Long-lived registers caused register pressure
- Reduced renaming pool for out-of-order execution
- Hurt instruction-level parallelism
- L1 cache hits are nearly free on modern ARM

### Phase 2: Localized Constant Loading (SUCCESS)

**Solution:** Load constants within macro, short lifetime
```assembly
.macro roundsm16_opt ...
    // Load here (short lifetime)
    load_const_addr .Linv_shift_row, x10, t4
    tbl x0.16b, {x0.16b}, t4.16b

    // ... use immediately ...

    // Post-filter loads overwrite t4 (natural cleanup)
    load_const_addr .Lpost_tf_lo_s1, x10, t0
.endm
```

**Result:** Performance restored to baseline (513.5 MiB/s)

**Key Learning:**
- Register pressure > instruction count
- Trust modern microarchitecture (OoO, HW prefetcher)
- Measure, don't assume!

---

## ABI Compliance (Critical for Production)

### AAPCS64 Requirements

**Callee-saved registers:**
- Vector: q8-q15 (lower 128 bits of v8-v15)
- GPR: x19-x28, x29 (FP), x30 (LR)

**Implementation:**
```assembly
camellia_encrypt_16blks_simd128:
    // Prologue: save callee-saved registers
    stp x29, x30, [sp, #-496]!
    mov x29, sp

    stp q8, q9, [sp, #16]
    stp q10, q11, [sp, #48]
    stp q12, q13, [sp, #80]
    stp q14, q15, [sp, #112]

    stp x19, x20, [sp, #144]
    // ... (save x21-x26)

    // ... function body ...

    // Epilogue: restore registers
    ldp x25, x26, [sp, #192]
    // ... (restore all)
    ldp x29, x30, [sp], #496
    ret
```

**Why it matters:**
- Prevents corruption of caller's data
- Required for integration with C/C++ code
- Debuggable with standard tools (GDB)

---

## Debugging Techniques

### 1. Hex Dumps

```c
void dump_state(const char *label, uint8x16_t vec[8]) {
    printf("%s:\n", label);
    for (int i = 0; i < 8; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < 16; j++)
            printf("%02x ", vec[i][j]);
        printf("\n");
    }
}
```

### 2. GDB with Assembly

```bash
gdb ./test_decrypt_basic
(gdb) break camellia_simd128_step3_asm
(gdb) run
(gdb) info registers v0 v1 v2 v3  # Inspect NEON registers
(gdb) x/32xb $x23                  # Examine memory (AB buffer)
```

### 3. Known Test Vectors

```c
// RFC 3713 test vectors
uint8_t key_128[16] = {0x01, 0x23, 0x45, ...};
uint8_t plaintext[16] = {0x01, 0x23, 0x45, ...};
uint8_t expected_cipher[16] = {0x67, 0x67, 0x31, ...};

// Test
encrypt(cipher, plaintext, key_128);
assert(memcmp(cipher, expected_cipher, 16) == 0);
```

---

## Lessons Learned

### Technical Insights

1. **AES-NI S-box Emulation Works**
   - AESE instruction can emulate Camellia S-boxes
   - Requires affine transform tables before/after
   - ShiftRows must be cancelled (use TBL with inverse pattern)

2. **FL/FLINV Key Swapping is Elegant**
   - No need to implement inverse operations
   - Decryption just swaps key order: `FL(kr, kl)` instead of `FL(kl, kr)`
   - Saves code size and complexity

3. **Register Pressure Matters More Than Instruction Count**
   - Modern CPUs hide memory latency well
   - Long-lived registers hurt out-of-order execution
   - Localized constants > function-level hoisting

4. **Incremental Testing is Essential**
   - Catch bugs early when code is small
   - Each stage validates previous stages
   - Debugging 18 rounds at once = nightmare

### Process Insights

1. **Write Test Wrapper First**
   - Define interface before implementation
   - C harness makes testing trivial
   - Printf debugging > GDB for state inspection

2. **Compare Against C Reference Constantly**
   - Byte-by-byte comparison catches subtle bugs
   - Known vectors validate end-to-end
   - Never trust "it looks right"

3. **Document Discoveries Immediately**
   - Future you will forget why decisions were made
   - Comments explain "why", not "what"
   - Architecture-specific quirks must be noted

4. **ABI Compliance From Day One**
   - Retrofitting ABI compliance is painful
   - Stack frame design affects everything
   - Callee-saved registers = non-negotiable

---

## Repository Structure for GitHub

```
camellia-simd-aesni/
├── README.md                          # Project overview, features, usage
├── LICENSE                            # BSD/MIT license
├── Makefile                           # Build system
├── bench.mk                           # Benchmark-specific build
│
├── camellia_simd.h                    # Public API header
├── camellia_simd128_with_aes_instruction_set.c  # C reference
├── camellia_simd128_aarch64_ce.S      # AArch64 assembly (this file!)
├── camellia_simd128_x86-64_aesni_avx.S    # x86-64 implementation
│
├── test_decrypt_basic.c               # Core functionality tests
├── main.c                             # Example usage
│
├── tools/
│   ├── bench_camellia_a64.c           # Performance benchmark
│   ├── bench_all_a64.sh               # Automated benchmark script
│   └── README.md                      # Benchmark usage guide
│
└── docs/
    ├── DEVELOPMENT_APPROACH.md        # This file
    ├── BENCHMARK.md                   # Performance results
    └── ABI_COMPLIANCE.md              # AAPCS64 notes
```

---

## Building and Testing

### Quick Start

```bash
# Build everything
make

# Run functionality tests
./test_decrypt_basic

# Run performance benchmark (requires AArch64)
make -f bench.mk bench
./bench_camellia_a64 --impl asm --op enc --ks 128 --bytes 2Gi --batch 16
```

### Benchmark Results (AWS Graviton3)

| Implementation | Throughput | vs C Reference |
|----------------|-----------|----------------|
| C Reference    | 575 MiB/s | Baseline       |
| ASM (AArch64)  | 513 MiB/s | -11%           |

*Gap analysis: Compiler scheduling advantage, still investigating.*

---

## Future Work

### Performance Optimization

1. **Instruction Scheduling**
   - Manual interleaving of independent operations
   - Hide load latency by moving loads earlier
   - Potential 2-5% improvement

2. **Post-Transform Table Hoisting**
   - Share post-filter tables across 2-round pairs
   - Reduce memory loads without register pressure
   - Potential 1-3% improvement

3. **Study Compiler Output**
   - Analyze C reference assembly
   - Identify superior patterns
   - Apply insights to hand-written code

### Platform Support

1. **Apple Silicon Optimization**
   - M1/M2/M3 have different microarchitecture
   - May benefit from different register allocation
   - Test on actual hardware

2. **AWS Graviton4**
   - Newer core with improved vector units
   - May have different performance characteristics

---

## Conclusion

This incremental, test-driven approach enabled development of a **correct, maintainable, and reasonably performant** AArch64 implementation. While the assembly doesn't beat the C compiler's optimization (12% gap remains), it demonstrates:

- ✅ Correct implementation (all test vectors pass)
- ✅ ABI compliance (safe for production use)
- ✅ Maintainable code (well-documented, structured)
- ✅ Reasonable performance (within 15% of C reference)

The development methodology—**build small, test immediately, document discoveries**—proved essential for managing the complexity of hand-written SIMD assembly.

---

**End of Document**
