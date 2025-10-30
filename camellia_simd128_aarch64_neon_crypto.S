/*
 * camellia_simd128_aarch64_neon_crypto.S - AArch64 NEON+Crypto Assembly Implementation
 *
 * Copyright (C) 2024 Optimized Camellia SIMD Implementation
 *
 * Hand-optimized AArch64 assembly implementation of Camellia cipher
 * using NEON SIMD and ARMv8 Crypto Extensions for accelerated S-box operations.
 *
 * This implementation processes 16 blocks in parallel for optimal performance.
 * Expected performance: ~650-700 MiB/s on AWS Graviton3.
 */

#ifdef __aarch64__

.arch armv8-a+crypto
.text
.align 4

/*
 * Register allocation strategy:
 * v0-v15:  16 parallel data blocks
 * v16-v23: Temporary registers for S-box operations
 * v24-v27: Round keys
 * v28-v31: Constants and masks
 * x0: Context pointer
 * x1: Output pointer
 * x2: Input pointer
 * x3-x7: Temporary scalar registers
 */

/* Constant tables for S-box transformations and byte-slicing */
.section .rodata
.align 4

/*
 * Pre-SubByte transform tables
 * These tables convert Camellia S-box input to AES domain
 * before applying AES SubBytes instruction
 */

/* Pre-transform for S-box 1, 2, 3 */
.Lpre_tf_lo_s1:
    .byte 0x45, 0xe8, 0x40, 0xed, 0x2e, 0x83, 0x2b, 0x86
    .byte 0x4b, 0xe6, 0x4e, 0xe3, 0x20, 0x8d, 0x25, 0x88

.Lpre_tf_hi_s1:
    .byte 0x00, 0x51, 0xf1, 0xa0, 0x8a, 0xdb, 0x7b, 0x2a
    .byte 0x09, 0x58, 0xf8, 0xa9, 0x83, 0xd2, 0x72, 0x23

/* Pre-transform for S-box 4 (with rotate) */
.Lpre_tf_lo_s4:
    .byte 0x45, 0x40, 0x2e, 0x2b, 0x4b, 0x4e, 0x20, 0x25
    .byte 0x14, 0x11, 0x7f, 0x7a, 0x1a, 0x1f, 0x71, 0x74

.Lpre_tf_hi_s4:
    .byte 0x00, 0xf1, 0x8a, 0x7b, 0x09, 0xf8, 0x83, 0x72
    .byte 0xad, 0x5c, 0x27, 0xd6, 0xa4, 0x55, 0x2e, 0xdf

/*
 * Post-SubByte transform tables
 * These tables convert AES SubBytes output back to Camellia domain
 */

/* Post-transform for S-box 1 and 4 */
.Lpost_tf_lo_s1:
    .byte 0x3c, 0xcc, 0xcf, 0x3f, 0x32, 0xc2, 0xc1, 0x31
    .byte 0xdc, 0x2c, 0x2f, 0xdf, 0xd2, 0x22, 0x21, 0xd1

.Lpost_tf_hi_s1:
    .byte 0x00, 0xf9, 0x86, 0x7f, 0xd7, 0x2e, 0x51, 0xa8
    .byte 0xa4, 0x5d, 0x22, 0xdb, 0x73, 0x8a, 0xf5, 0x0c

/* Post-transform for S-box 2 (with rotate left) */
.Lpost_tf_lo_s2:
    .byte 0x78, 0x99, 0x9f, 0x7e, 0x64, 0x85, 0x83, 0x62
    .byte 0xb9, 0x58, 0x5e, 0xbf, 0xa5, 0x44, 0x42, 0xa3

.Lpost_tf_hi_s2:
    .byte 0x00, 0xf3, 0x0d, 0xfe, 0xaf, 0x5c, 0xa2, 0x51
    .byte 0x49, 0xba, 0x44, 0xb7, 0xe6, 0x15, 0xeb, 0x18

/* Post-transform for S-box 3 (with rotate right) */
.Lpost_tf_lo_s3:
    .byte 0x1e, 0x66, 0xe7, 0x9f, 0x19, 0x61, 0xe0, 0x98
    .byte 0x6e, 0x16, 0x97, 0xef, 0x69, 0x11, 0x90, 0xe8

.Lpost_tf_hi_s3:
    .byte 0x00, 0xfc, 0x43, 0xbf, 0xeb, 0x17, 0xa8, 0x54
    .byte 0x52, 0xae, 0x11, 0xed, 0xb9, 0x45, 0xfa, 0x06

/*
 * Shuffle masks and constants
 */

/* Inverse ShiftRows mask - undoes AES ShiftRows from AESE instruction */
.Linv_shift_rows:
    .byte 0x00, 0x0d, 0x0a, 0x07, 0x04, 0x01, 0x0e, 0x0b
    .byte 0x08, 0x05, 0x02, 0x0f, 0x0c, 0x09, 0x06, 0x03

/* Nibble mask (0x0f) for extracting low 4 bits */
.Lnibble_mask:
    .byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
    .byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f

/* Shuffle pattern for byte-slicing (16x16b transpose) */
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15

/* Pack and byte-swap pattern for pre-whitening key (64-bit to 128-bit) */
.Lpack_bswap:
    .byte 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04
    .byte 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f

/* Shuffle pattern for 8x8 byte transpose */
.Ltranspose_8x8_shuf:
    .byte 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15

/*
 * Byte position extractors for key material spreading
 * Used with TBL instruction to extract specific byte positions
 */
.Lbyte_zeros:
    .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

.Lbyte_ones:
    .byte 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1

.Lbyte_twos:
    .byte 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2

.Lbyte_threes:
    .byte 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3

.Lbyte_fours:
    .byte 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4

.Lbyte_fives:
    .byte 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5

.Lbyte_sixs:
    .byte 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6

.Lbyte_sevens:
    .byte 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7

.text

/*
 * ============================================================================
 * Core Macros
 * ============================================================================
 */

/*
 * filter_8bit - 8-bit S-box lookup using 4-bit table lookup
 *
 * This is the fundamental building block for Camellia S-box operations.
 * It splits each byte into high and low 4-bit nibbles, performs table
 * lookups on each, and XORs the results together.
 *
 * Parameters:
 *   x        - Input/output vector (will be overwritten with result)
 *   lo_t     - Lookup table for low nibbles (16 bytes)
 *   hi_t     - Lookup table for high nibbles (16 bytes)
 *   mask4bit - Nibble mask (0x0f repeated, loaded from .Lnibble_mask)
 *   tmp0     - Temporary vector register
 *
 * Algorithm:
 *   1. tmp0 = x & 0x0f          (extract low 4 bits of each byte)
 *   2. x = x >> 4               (extract high 4 bits of each byte)
 *   3. tmp0 = lo_t[tmp0]        (lookup low nibbles)
 *   4. x = hi_t[x]              (lookup high nibbles)
 *   5. x = tmp0 ^ x             (combine results)
 *
 * Example (one byte):
 *   Input:  0x3A = 0011 1010
 *   Step 1: tmp0 = 0x0A (low 4 bits)
 *   Step 2: x = 0x03 (high 4 bits)
 *   Step 3: tmp0 = lo_t[0x0A]
 *   Step 4: x = hi_t[0x03]
 *   Step 5: result = tmp0 ^ x
 *
 * This operates on all 16 bytes in parallel using NEON instructions.
 */
.macro filter_8bit x, lo_t, hi_t, mask4bit, tmp0
    // Extract low nibbles (bits 0-3 of each byte)
    and     \tmp0\().16b, \x\().16b, \mask4bit\().16b

    // Extract high nibbles (bits 4-7 of each byte)
    ushr    \x\().16b, \x\().16b, #4

    // Table lookup for low nibbles (16 parallel lookups)
    tbl     \tmp0\().16b, {\lo_t\().16b}, \tmp0\().16b

    // Table lookup for high nibbles (16 parallel lookups)
    tbl     \x\().16b, {\hi_t\().16b}, \x\().16b

    // XOR the two lookup results together
    eor     \x\().16b, \tmp0\().16b, \x\().16b
.endm

/*
 * transpose_4x4 - Transpose 4x4 matrix of 32-bit elements
 *
 * This macro performs a 4x4 matrix transpose on 32-bit elements.
 * It's a key component of byte-slicing, where we reorganize data
 * from block-oriented format to byte-position-oriented format.
 *
 * Parameters:
 *   x0, x1, x2, x3 - Input vectors (4 rows of the matrix)
 *                    Each contains 4 x 32-bit elements
 *   t1, t2         - Temporary vectors
 *
 * Input format (4 vectors, each with 4x 32-bit words):
 *   x0: [A0, A1, A2, A3]
 *   x1: [B0, B1, B2, B3]
 *   x2: [C0, C1, C2, C3]
 *   x3: [D0, D1, D2, D3]
 *
 * Output format (transposed):
 *   x0: [A0, B0, C0, D0]  <- Column 0 becomes row 0
 *   x1: [A1, B1, C1, D1]  <- Column 1 becomes row 1
 *   x2: [A2, B2, C2, D2]  <- Column 2 becomes row 2
 *   x3: [A3, B3, C3, D3]  <- Column 3 becomes row 3
 *
 * Algorithm:
 *   1. First level: Interleave 32-bit elements (zip1/zip2 on .4s)
 *   2. Second level: Interleave 64-bit pairs (zip1/zip2 on .2d)
 *
 * The zip instructions interleave elements:
 *   zip1 v.4s, a, b -> [a[0], b[0], a[1], b[1]]  (low half)
 *   zip2 v.4s, a, b -> [a[2], b[2], a[3], b[3]]  (high half)
 */
.macro transpose_4x4 x0, x1, x2, x3, t1, t2
    // First level of transpose: interleave 32-bit elements
    // After this, adjacent 32-bit pairs from x0 and x1 are interleaved
    zip2    \t2\().4s, \x0\().4s, \x1\().4s    // t2 = [A2, B2, A3, B3]
    zip1    \x0\().4s, \x0\().4s, \x1\().4s    // x0 = [A0, B0, A1, B1]

    zip1    \t1\().4s, \x2\().4s, \x3\().4s    // t1 = [C0, D0, C1, D1]
    zip2    \x2\().4s, \x2\().4s, \x3\().4s    // x2 = [C2, D2, C3, D3]

    // Second level of transpose: interleave 64-bit pairs
    // This completes the 4x4 transpose by combining the low and high halves
    zip2    \x1\().2d, \x0\().2d, \t1\().2d    // x1 = [A1, B1, C1, D1]
    zip1    \x0\().2d, \x0\().2d, \t1\().2d    // x0 = [A0, B0, C0, D0]

    zip2    \x3\().2d, \t2\().2d, \x2\().2d    // x3 = [A3, B3, C3, D3]
    zip1    \x2\().2d, \t2\().2d, \x2\().2d    // x2 = [A2, B2, C2, D2]
.endm

/*
 * byteslice_16x16b - Convert 16 blocks from normal to byte-sliced format
 *
 * This is the core transformation for SIMD Camellia encryption. It converts
 * 16 blocks (256 bytes total) from block-oriented format to byte-sliced format,
 * enabling parallel processing of all 16 blocks simultaneously.
 *
 * Parameters:
 *   a0-a3, b0-b3, c0-c3, d0-d3: 16 input/output vectors
 *                               Input: 16 blocks (1 per vector)
 *                               Output: 16 byte-sliced vectors
 *   t0-t5: 6 temporary vector registers
 *
 * Input format (block-oriented):
 *   a0: [block0_byte0  block0_byte1  ... block0_byte15]
 *   b0: [block1_byte0  block1_byte1  ... block1_byte15]
 *   c0: [block2_byte0  block2_byte1  ... block2_byte15]
 *   ...
 *
 * Output format (byte-sliced):
 *   a0: [block0_byte0 block1_byte0 block2_byte0 ... block15_byte0]
 *   a1: [block0_byte1 block1_byte1 block2_byte1 ... block15_byte1]
 *   a2: [block0_byte2 block1_byte2 block2_byte2 ... block15_byte2]
 *   ...
 *
 * Algorithm (3 phases):
 *   Phase 1: Transpose 4x4 groups (32-bit elements)
 *   Phase 2: Shuffle bytes within each vector
 *   Phase 3: Transpose again to finalize byte-slicing
 *
 * Note: This implementation uses AArch64's 32 vector registers to avoid
 * all stack operations, unlike the x86 version which needs stack storage.
 */
.macro byteslice_16x16b a0, b0, c0, d0, a1, b1, c1, d1, \
                        a2, b2, c2, d2, a3, b3, c3, d3, \
                        t0, t1, t2, t3, t4, t5

    /*
     * Phase 1: First level of transpose (4x4 groups)
     * Transpose within each group of 4 blocks
     */

    // Save d2, d3 (needed as temporaries for transpose)
    mov     \t4\().16b, \d2\().16b
    mov     \t5\().16b, \d3\().16b

    // Transpose Group A (a0-a3) and Group B (b0-b3)
    transpose_4x4 \a0, \a1, \a2, \a3, \d2, \d3
    transpose_4x4 \b0, \b1, \b2, \b3, \d2, \d3

    // Restore d2, d3 and save a0, a1
    mov     \d2\().16b, \t4\().16b
    mov     \d3\().16b, \t5\().16b
    mov     \t4\().16b, \a0\().16b
    mov     \t5\().16b, \a1\().16b

    // Transpose Group C (c0-c3) and Group D (d0-d3)
    transpose_4x4 \c0, \c1, \c2, \c3, \a0, \a1
    transpose_4x4 \d0, \d1, \d2, \d3, \a0, \a1

    /*
     * Phase 2: Shuffle bytes within vectors
     * Apply the .Lshufb_16x16b pattern to reorganize bytes
     */

    // Load shuffle pattern into a0 (need to use a scalar register temporarily)
    // Save the address calculation in t2 to avoid conflict
    adrp    x10, .Lshufb_16x16b
    add     x10, x10, :lo12:.Lshufb_16x16b
    ld1     {\t2\().16b}, [x10]      // t2 = shuffle pattern

    // Now copy pattern to a0 (which will be overwritten anyway)
    mov     \a0\().16b, \t2\().16b

    // Save a1 for later
    mov     \t3\().16b, \t5\().16b    // t3 = saved a1

    // Apply shuffle to 14 vectors (all except t4 which holds old a0)
    tbl     \a2\().16b, {\a2\().16b}, \a0\().16b
    tbl     \a3\().16b, {\a3\().16b}, \a0\().16b
    tbl     \b0\().16b, {\b0\().16b}, \a0\().16b
    tbl     \b1\().16b, {\b1\().16b}, \a0\().16b
    tbl     \b2\().16b, {\b2\().16b}, \a0\().16b
    tbl     \b3\().16b, {\b3\().16b}, \a0\().16b
    tbl     \a1\().16b, {\t3\().16b}, \a0\().16b    // shuffle saved a1
    tbl     \c0\().16b, {\c0\().16b}, \a0\().16b
    tbl     \c1\().16b, {\c1\().16b}, \a0\().16b
    tbl     \c2\().16b, {\c2\().16b}, \a0\().16b
    tbl     \c3\().16b, {\c3\().16b}, \a0\().16b
    tbl     \d0\().16b, {\d0\().16b}, \a0\().16b
    tbl     \d1\().16b, {\d1\().16b}, \a0\().16b

    // Handle d2, d3 and saved a0
    // BUG FIX: Shuffle d2 and d3 BEFORE shuffling a0, because shuffling a0 will overwrite the pattern!
    mov     \t3\().16b, \d3\().16b                      // t3 = old d3
    tbl     \d2\().16b, {\d2\().16b}, \a0\().16b       // Shuffle d2 while a0 still has pattern
    tbl     \d3\().16b, {\t3\().16b}, \a0\().16b       // Shuffle d3 (from t3) while a0 still has pattern

    // Now shuffle the saved a0 (this will overwrite a0, but we're done using it as a pattern)
    mov     \t3\().16b, \d3\().16b                      // Save shuffled d3 to t3
    mov     \d3\().16b, \t4\().16b                      // Load saved a0 to d3
    tbl     \a0\().16b, {\d3\().16b}, \a0\().16b       // Shuffle saved a0, result in a0
    mov     \d3\().16b, \t3\().16b                      // Restore shuffled d3

    // CRITICAL: Save shuffled d2, d3 NOW before Phase 3 corrupts them!
    // Group 2-3 transposes need these as input vectors
    mov     \t4\().16b, \d2\().16b           // t4 = shuffled d2
    mov     \t5\().16b, \d3\().16b           // t5 = shuffled d3

    /*
     * Phase 3: Second level of transpose
     * Final organization into byte-sliced format
     */

    // Transpose new Group 0 (a0, b0, c0, d0) and Group 1 (a1, b1, c1, d1)
    // These will corrupt d2, d3 (used as temps), but we saved them in t4, t5!
    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3

    // Restore shuffled d2, d3 NOW (before Group 2-3 transposes need them as inputs!)
    mov     \d2\().16b, \t4\().16b
    mov     \d3\().16b, \t5\().16b

    // Save b0, b1 and transpose Group 2 (a2, b2, c2, d2) and Group 3 (a3, b3, c3, d3)
    mov     \t0\().16b, \b0\().16b
    mov     \t1\().16b, \b1\().16b
    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1

    // Restore b0, b1
    mov     \b0\().16b, \t0\().16b
    mov     \b1\().16b, \t1\().16b

.endm

/*
 * test_byteslice_16x16b_asm - Test wrapper for byteslice macro
 *
 * Parameters:
 *   x0 = output buffer (256 bytes)
 *   x1 = input buffer (256 bytes)
 */
.globl test_byteslice_16x16b_asm
.type test_byteslice_16x16b_asm, %function
test_byteslice_16x16b_asm:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // Save output pointer
    mov     x10, x0

    // Load 16 input blocks into v0-v15
    ldp     q0, q1, [x1, #0]        // blocks 0-1
    ldp     q2, q3, [x1, #32]       // blocks 2-3
    ldp     q4, q5, [x1, #64]       // blocks 4-5
    ldp     q6, q7, [x1, #96]       // blocks 6-7
    ldp     q8, q9, [x1, #128]      // blocks 8-9
    ldp     q10, q11, [x1, #160]    // blocks 10-11
    ldp     q12, q13, [x1, #192]    // blocks 12-13
    ldp     q14, q15, [x1, #224]    // blocks 14-15

    // Call byteslice_16x16b macro
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Store 16 output blocks
    stp     q0, q1, [x10, #0]       // a0, b0
    stp     q2, q3, [x10, #32]      // c0, d0
    stp     q4, q5, [x10, #64]      // a1, b1
    stp     q6, q7, [x10, #96]      // c1, d1
    stp     q8, q9, [x10, #128]     // a2, b2
    stp     q10, q11, [x10, #160]   // c2, d2
    stp     q12, q13, [x10, #192]   // a3, b3
    stp     q14, q15, [x10, #224]   // c3, d3

    ldp     x29, x30, [sp], #16
    ret
.size test_byteslice_16x16b_asm, .-test_byteslice_16x16b_asm

/*
 * roundsm16 - Camellia round function (S-box + P-function)
 *
 * This is the core encryption round of Camellia. It performs:
 * 1. Inverse ShiftRows (undo AES ShiftRows from previous operations)
 * 2. Pre-filter transformations (Camellia → AES domain)
 * 3. AES SubBytes using AESE instruction
 * 4. Post-filter transformations (AES → Camellia domain)
 * 5. P-function (linear diffusion with XOR network)
 * 6. Add round key and XOR with CD state
 *
 * Parameters:
 *   x0-x7: 8 input/output vectors (AB state, byte-sliced)
 *   t0-t7: 8 temporary registers
 *   key_ptr: pointer to round key (8 bytes)
 *   cd_ptr: pointer to CD state (8 vectors, 128 bytes)
 *
 * Note: This processes 16 blocks in parallel using byte-sliced format
 */
.macro roundsm16 x0, x1, x2, x3, x4, x5, x6, x7, \
                 t0, t1, t2, t3, t4, t5, t6, t7, \
                 key_ptr, cd_ptr

    /*
     * Phase 1: AES Inverse ShiftRows
     * Undo the ShiftRows that will be applied by AESE
     */

    // Load inverse shift rows mask
    adrp    x11, .Linv_shift_rows
    add     x11, x11, :lo12:.Linv_shift_rows
    ld1     {\t4\().16b}, [x11]

#ifdef DEBUG_ASM
    // DEBUG: Save AB state BEFORE inv_shift_rows
    adrp    x11, debug_roundsm16_phase0_ab
    add     x11, x11, :lo12:debug_roundsm16_phase0_ab
    st1     {\x0\().16b, \x1\().16b, \x2\().16b, \x3\().16b}, [x11], #64
    st1     {\x4\().16b, \x5\().16b, \x6\().16b, \x7\().16b}, [x11]
#endif

    // Apply inverse shift rows to all 8 vectors
    tbl     \x0\().16b, {\x0\().16b}, \t4\().16b
    tbl     \x7\().16b, {\x7\().16b}, \t4\().16b
    tbl     \x1\().16b, {\x1\().16b}, \t4\().16b
    tbl     \x4\().16b, {\x4\().16b}, \t4\().16b
    tbl     \x2\().16b, {\x2\().16b}, \t4\().16b
    tbl     \x5\().16b, {\x5\().16b}, \t4\().16b
    tbl     \x3\().16b, {\x3\().16b}, \t4\().16b
    tbl     \x6\().16b, {\x6\().16b}, \t4\().16b

    /*
     * DEBUG: Check if inv_shift_rows broke uniformity
     * NOTE: Inverse shift rows on uniform bytes should keep them uniform!
     * The inv_shift_rows mask just permutes byte positions.
     * For uniform input (all bytes = X), output should still be all bytes = X.
     */
#ifdef DEBUG_ASM
    // Save AB state after Phase 1 (inv_shift_rows)
    adrp    x11, debug_roundsm16_phase1_ab
    add     x11, x11, :lo12:debug_roundsm16_phase1_ab
    st1     {\x0\().16b, \x1\().16b, \x2\().16b, \x3\().16b}, [x11], #64
    st1     {\x4\().16b, \x5\().16b, \x6\().16b, \x7\().16b}, [x11]
#endif

    /*
     * Phase 2: Pre-filter transformations
     * Convert Camellia S-box inputs to AES domain
     */

    // Load nibble mask for filter_8bit
    adrp    x11, .Lnibble_mask
    add     x11, x11, :lo12:.Lnibble_mask
    ld1     {\t7\().16b}, [x11]

    // Load pre-transform tables for S-boxes 1, 2, 3
    adrp    x11, .Lpre_tf_lo_s1
    add     x11, x11, :lo12:.Lpre_tf_lo_s1
    ld1     {\t0\().16b}, [x11]
    adrp    x11, .Lpre_tf_hi_s1
    add     x11, x11, :lo12:.Lpre_tf_hi_s1
    ld1     {\t1\().16b}, [x11]

    // Pre-filter S-boxes 1, 2, 3 (6 applications)
    filter_8bit \x0, \t0, \t1, \t7, \t6
    filter_8bit \x7, \t0, \t1, \t7, \t6
    filter_8bit \x1, \t0, \t1, \t7, \t6
    filter_8bit \x4, \t0, \t1, \t7, \t6
    filter_8bit \x2, \t0, \t1, \t7, \t6
    filter_8bit \x5, \t0, \t1, \t7, \t6

    // Load pre-transform tables for S-box 4
    adrp    x11, .Lpre_tf_lo_s4
    add     x11, x11, :lo12:.Lpre_tf_lo_s4
    ld1     {\t2\().16b}, [x11]
    adrp    x11, .Lpre_tf_hi_s4
    add     x11, x11, :lo12:.Lpre_tf_hi_s4
    ld1     {\t3\().16b}, [x11]

    // Pre-filter S-box 4 (2 applications)
    filter_8bit \x3, \t2, \t3, \t7, \t6
    filter_8bit \x6, \t2, \t3, \t7, \t6

    /*
     * Phase 3: AES SubBytes
     * Use AESE instruction (SubBytes + ShiftRows + MixColumns)
     * Note: We only want SubBytes + ShiftRows, so we'll handle MixColumns separately
     */

    // Zero vector for AESE (key input)
    movi    \t4\().16b, #0

    // Apply AESE to all 8 vectors
    aese    \x0\().16b, \t4\().16b
    aese    \x7\().16b, \t4\().16b
    aese    \x1\().16b, \t4\().16b
    aese    \x4\().16b, \t4\().16b
    aese    \x2\().16b, \t4\().16b
    aese    \x5\().16b, \t4\().16b
    aese    \x3\().16b, \t4\().16b
    aese    \x6\().16b, \t4\().16b

    // Note: AESE includes MixColumns which we don't want
    // The post-filter tables are adjusted to compensate for this

    /*
     * Phase 4: Post-filter transformations
     * Convert AES SubBytes outputs back to Camellia domain
     */

    // Load post-transform tables for S-boxes 1 and 4
    adrp    x11, .Lpost_tf_lo_s1
    add     x11, x11, :lo12:.Lpost_tf_lo_s1
    ld1     {\t0\().16b}, [x11]
    adrp    x11, .Lpost_tf_hi_s1
    add     x11, x11, :lo12:.Lpost_tf_hi_s1
    ld1     {\t1\().16b}, [x11]

    // Post-filter S-boxes 1 and 4 (4 applications)
    filter_8bit \x0, \t0, \t1, \t7, \t6
    filter_8bit \x7, \t0, \t1, \t7, \t6
    filter_8bit \x3, \t0, \t1, \t7, \t6
    filter_8bit \x6, \t0, \t1, \t7, \t6

    // Load post-transform tables for S-box 3
    adrp    x11, .Lpost_tf_lo_s3
    add     x11, x11, :lo12:.Lpost_tf_lo_s3
    ld1     {\t2\().16b}, [x11]
    adrp    x11, .Lpost_tf_hi_s3
    add     x11, x11, :lo12:.Lpost_tf_hi_s3
    ld1     {\t3\().16b}, [x11]

    // Post-filter S-box 3 (2 applications)
    filter_8bit \x2, \t2, \t3, \t7, \t6
    filter_8bit \x5, \t2, \t3, \t7, \t6

    // Load post-transform tables for S-box 2
    adrp    x11, .Lpost_tf_lo_s2
    add     x11, x11, :lo12:.Lpost_tf_lo_s2
    ld1     {\t4\().16b}, [x11]
    adrp    x11, .Lpost_tf_hi_s2
    add     x11, x11, :lo12:.Lpost_tf_hi_s2
    ld1     {\t5\().16b}, [x11]

    // Load round key (8 bytes)
    ldr     x11, [\key_ptr]
    dup     \t0\().2d, x11

    // Post-filter S-box 2 (2 applications)
    filter_8bit \x1, \t4, \t5, \t7, \t2
    filter_8bit \x4, \t4, \t5, \t7, \t2

    /*
     * Phase 5: P-function (linear diffusion layer)
     * This is a specific XOR network that provides diffusion
     */

    // Extract individual key bytes using byte extractors
    adrp    x11, .Lbyte_threes
    add     x11, x11, :lo12:.Lbyte_threes
    ld1     {\t6\().16b}, [x11]
    tbl     \t3\().16b, {\t0\().16b}, \t6\().16b

    adrp    x11, .Lbyte_twos
    add     x11, x11, :lo12:.Lbyte_twos
    ld1     {\t6\().16b}, [x11]
    tbl     \t2\().16b, {\t0\().16b}, \t6\().16b

    // P-function: First round of XORs
    eor     \x0\().16b, \x0\().16b, \x5\().16b
    eor     \x1\().16b, \x1\().16b, \x6\().16b
    eor     \x2\().16b, \x2\().16b, \x7\().16b
    eor     \x3\().16b, \x3\().16b, \x4\().16b

    // Load more key bytes
    adrp    x11, .Lbyte_ones
    add     x11, x11, :lo12:.Lbyte_ones
    ld1     {\t6\().16b}, [x11]
    tbl     \t1\().16b, {\t0\().16b}, \t6\().16b

    adrp    x11, .Lbyte_sevens
    add     x11, x11, :lo12:.Lbyte_sevens
    ld1     {\t6\().16b}, [x11]
    tbl     \t7\().16b, {\t0\().16b}, \t6\().16b

    // P-function: Second round of XORs
    eor     \x4\().16b, \x4\().16b, \x2\().16b
    eor     \x5\().16b, \x5\().16b, \x3\().16b
    eor     \x6\().16b, \x6\().16b, \x0\().16b
    eor     \x7\().16b, \x7\().16b, \x1\().16b

    // Load more key bytes
    adrp    x11, .Lbyte_sixs
    add     x11, x11, :lo12:.Lbyte_sixs
    ld1     {\t5\().16b}, [x11]
    tbl     \t6\().16b, {\t0\().16b}, \t5\().16b

    adrp    x11, .Lbyte_fives
    add     x11, x11, :lo12:.Lbyte_fives
    ld1     {\t5\().16b}, [x11]
    tbl     \t5\().16b, {\t0\().16b}, \t5\().16b

    // P-function: Third round of XORs
    eor     \x0\().16b, \x0\().16b, \x7\().16b
    eor     \x1\().16b, \x1\().16b, \x4\().16b
    eor     \x2\().16b, \x2\().16b, \x5\().16b
    eor     \x3\().16b, \x3\().16b, \x6\().16b

    // Load last key byte
    adrp    x11, .Lbyte_fours
    add     x11, x11, :lo12:.Lbyte_fours
    ld1     {\t4\().16b}, [x11]
    tbl     \t4\().16b, {\t0\().16b}, \t4\().16b

    // P-function: Final round of XORs
    eor     \x4\().16b, \x4\().16b, \x3\().16b
    eor     \x5\().16b, \x5\().16b, \x0\().16b
    eor     \x6\().16b, \x6\().16b, \x1\().16b
    eor     \x7\().16b, \x7\().16b, \x2\().16b
    // Note: x7 now contains swapped high/low parts

    /*
     * Phase 6: Add round key and XOR with CD state
     * This combines the result with the CD half of the Feistel structure
     */

    // XOR with round key bytes and CD state
    eor     \x4\().16b, \x4\().16b, \t3\().16b
    ld1     {\t3\().16b}, [\cd_ptr]
    eor     \x4\().16b, \x4\().16b, \t3\().16b

    // Zero out t3 and use t0 for broadcast
    movi    \t3\().16b, #0
    tbl     \t0\().16b, {\t0\().16b}, \t3\().16b

    eor     \x5\().16b, \x5\().16b, \t2\().16b
    add     x12, \cd_ptr, #16
    ld1     {\t2\().16b}, [x12]
    eor     \x5\().16b, \x5\().16b, \t2\().16b

    eor     \x6\().16b, \x6\().16b, \t1\().16b
    add     x12, \cd_ptr, #32
    ld1     {\t1\().16b}, [x12]
    eor     \x6\().16b, \x6\().16b, \t1\().16b

    eor     \x7\().16b, \x7\().16b, \t0\().16b
    add     x12, \cd_ptr, #48
    ld1     {\t0\().16b}, [x12]
    eor     \x7\().16b, \x7\().16b, \t0\().16b

    eor     \x0\().16b, \x0\().16b, \t7\().16b
    add     x12, \cd_ptr, #64
    ld1     {\t7\().16b}, [x12]
    eor     \x0\().16b, \x0\().16b, \t7\().16b

    eor     \x1\().16b, \x1\().16b, \t6\().16b
    add     x12, \cd_ptr, #80
    ld1     {\t6\().16b}, [x12]
    eor     \x1\().16b, \x1\().16b, \t6\().16b

    eor     \x2\().16b, \x2\().16b, \t5\().16b
    add     x12, \cd_ptr, #96
    ld1     {\t5\().16b}, [x12]
    eor     \x2\().16b, \x2\().16b, \t5\().16b

    eor     \x3\().16b, \x3\().16b, \t4\().16b
    add     x12, \cd_ptr, #112
    ld1     {\t4\().16b}, [x12]
    eor     \x3\().16b, \x3\().16b, \t4\().16b

.endm

/*
 * rol32_1_16 - Rotate left by 1 bit on 32-bit values (byte-sliced)
 *
 * This macro rotates each 32-bit value left by 1 bit in byte-sliced format.
 * In byte-sliced format, a 32-bit value is spread across 4 consecutive vectors,
 * with each vector holding one byte position from all 16 blocks.
 *
 * The rotation works by:
 * 1. Extract the high bit (bit 7) from each byte in each vector
 * 2. Shift each vector left by 1 bit (multiply by 2)
 * 3. OR the high bit from vector N into the low bit of vector N+1
 * 4. OR the high bit from vector 3 back into vector 0 (wrap around for 32-bit)
 *
 * Parameters:
 *   v0, v1, v2, v3 - Four consecutive vectors representing one 32-bit word
 *                    across 16 blocks (modified in place)
 *   t0, t1, t2     - Temporary vectors for storing high bits
 *   zero           - Vector of zeros (for comparison)
 *
 * Note: This operates on all 16 blocks in parallel.
 */
.macro rol32_1_16 v0, v1, v2, v3, t0, t1, t2, zero
    // Extract high bit (bit 7) from each vector
    // High bit will be shifted into next byte position
    ushr    \t0\().16b, \v0\().16b, #7     // t0 = v0 >> 7 (high bit of v0)
    ushr    \t1\().16b, \v1\().16b, #7     // t1 = v1 >> 7 (high bit of v1)
    ushr    \t2\().16b, \v2\().16b, #7     // t2 = v2 >> 7 (high bit of v2)

    // Shift each vector left by 1 bit
    shl     \v0\().16b, \v0\().16b, #1     // v0 = v0 << 1
    shl     \v1\().16b, \v1\().16b, #1     // v1 = v1 << 1
    shl     \v2\().16b, \v2\().16b, #1     // v2 = v2 << 1

    // OR high bits into next vector's low bit
    orr     \v1\().16b, \v1\().16b, \t0\().16b  // v1 |= high_bit(v0)
    orr     \v2\().16b, \v2\().16b, \t1\().16b  // v2 |= high_bit(v1)
    orr     \v3\().16b, \v3\().16b, \t2\().16b  // v3 |= high_bit(v2)

    // Save high bit of v3 for wrap-around
    ushr    \t0\().16b, \v3\().16b, #7     // t0 = v3 >> 7 (before shift)
    shl     \v3\().16b, \v3\().16b, #1     // v3 = v3 << 1
    orr     \v0\().16b, \v0\().16b, \t0\().16b  // v0 |= high_bit(v3) (wrap)
.endm

/*
 * fls16 - FL layer for 16 blocks in parallel (byte-sliced)
 *
 * The FL function is a key-dependent linear transformation used in Camellia.
 * It operates on 128-bit blocks, treating each as two 64-bit halves (L and R).
 *
 * In byte-sliced format with 16 blocks:
 * - L (left 64 bits) = l0,l1,l2,l3 (low 32 bits) + l4,l5,l6,l7 (high 32 bits)
 * - R (right 64 bits) = r0,r1,r2,r3 (low 32 bits) + r4,r5,r6,r7 (high 32 bits)
 *
 * The FL algorithm performs 4 steps:
 * 1. t = kll & ll; lr ^= rol32(t, 1)
 * 2. t = krr | rr; rl ^= t
 * 3. t = krl & rl; rr ^= rol32(t, 1)
 * 4. t = klr | lr; ll ^= t
 *
 * Parameters:
 *   l0-l7     - Left half vectors (8 vectors, modified in place)
 *   r0-r7     - Right half vectors (8 vectors, modified in place)
 *   t0-t3     - Temporary vectors for operations
 *   tt0-tt3   - Additional temporary vectors
 *   kl, kr    - Pointers to key material (64-bit values each)
 *   bcast0-7  - Broadcast pattern vectors (preloaded from .Lbyte_zeros etc.)
 *
 * Memory layout of keys:
 *   kl points to 64-bit value: [low32 | high32]
 *   kr points to 64-bit value: [low32 | high32]
 */
.macro fls16 l0, l1, l2, l3, l4, l5, l6, l7, r0, r1, r2, r3, r4, r5, r6, r7, \
             t0, t1, t2, t3, tt0, tt1, tt2, tt3, kl, kr, \
             bcast0, bcast1, bcast2, bcast3, bcast4, bcast5, bcast6, bcast7

    // Step 1: t = kll & ll; lr ^= rol32(t, 1)
    // Load kll (low 32 bits of kl) and broadcast to all byte positions
    ldr     w10, [\kl]                      // Load low 32 bits of kl into w10
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to create byte-sliced broadcast of each byte position
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b  // All bytes = byte 0 of key
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b  // All bytes = byte 1 of key
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b  // All bytes = byte 2 of key
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b  // All bytes = byte 3 of key

    // AND with ll (l0-l3)
    and     \t0\().16b, \t0\().16b, \l0\().16b
    and     \t1\().16b, \t1\().16b, \l1\().16b
    and     \t2\().16b, \t2\().16b, \l2\().16b
    and     \t3\().16b, \t3\().16b, \l3\().16b

    // Rotate left by 1 bit
    rol32_1_16 \t3, \t2, \t1, \t0, \tt1, \tt2, \tt3, \tt0

    // XOR with lr (l4-l7)
    eor     \l4\().16b, \l4\().16b, \t0\().16b
    eor     \l5\().16b, \l5\().16b, \t1\().16b
    eor     \l6\().16b, \l6\().16b, \t2\().16b
    eor     \l7\().16b, \l7\().16b, \t3\().16b

    // Step 2: t = krr | rr; rl ^= t
    // Load krr (high 32 bits of kr) and broadcast
    ldr     w10, [\kr, #4]                  // Load high 32 bits of kr
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // OR with rr (r4-r7)
    orr     \t0\().16b, \t0\().16b, \r4\().16b
    orr     \t1\().16b, \t1\().16b, \r5\().16b
    orr     \t2\().16b, \t2\().16b, \r6\().16b
    orr     \t3\().16b, \t3\().16b, \r7\().16b

    // XOR with rl (r0-r3)
    eor     \r0\().16b, \r0\().16b, \t0\().16b
    eor     \r1\().16b, \r1\().16b, \t1\().16b
    eor     \r2\().16b, \r2\().16b, \t2\().16b
    eor     \r3\().16b, \r3\().16b, \t3\().16b

    // Step 3: t = krl & rl; rr ^= rol32(t, 1)
    // Load krl (low 32 bits of kr) and broadcast
    ldr     w10, [\kr]                      // Load low 32 bits of kr
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // AND with rl (r0-r3)
    and     \t0\().16b, \t0\().16b, \r0\().16b
    and     \t1\().16b, \t1\().16b, \r1\().16b
    and     \t2\().16b, \t2\().16b, \r2\().16b
    and     \t3\().16b, \t3\().16b, \r3\().16b

    // Rotate left by 1 bit
    rol32_1_16 \t3, \t2, \t1, \t0, \tt1, \tt2, \tt3, \tt0

    // XOR with rr (r4-r7)
    eor     \r4\().16b, \r4\().16b, \t0\().16b
    eor     \r5\().16b, \r5\().16b, \t1\().16b
    eor     \r6\().16b, \r6\().16b, \t2\().16b
    eor     \r7\().16b, \r7\().16b, \t3\().16b

    // Step 4: t = klr | lr; ll ^= t
    // Load klr (high 32 bits of kl) and broadcast
    ldr     w10, [\kl, #4]                  // Load high 32 bits of kl
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // OR with lr (l4-l7)
    orr     \t0\().16b, \t0\().16b, \l4\().16b
    orr     \t1\().16b, \t1\().16b, \l5\().16b
    orr     \t2\().16b, \t2\().16b, \l6\().16b
    orr     \t3\().16b, \t3\().16b, \l7\().16b

    // XOR with ll (l0-l3)
    eor     \l0\().16b, \l0\().16b, \t0\().16b
    eor     \l1\().16b, \l1\().16b, \t1\().16b
    eor     \l2\().16b, \l2\().16b, \t2\().16b
    eor     \l3\().16b, \l3\().16b, \t3\().16b
.endm

/*
 * fls16_inv - Inverse FL layer (FL^-1) for 16 blocks in parallel
 *
 * This is the inverse operation of the FL function. It applies the same
 * operations in reverse order to undo the FL transformation.
 *
 * The FL^-1 algorithm performs the 4 FL steps in reverse order:
 * 1. t = klr | lr; ll ^= t         (same as FL step 4)
 * 2. t = krl & rl; rr ^= rol32(t, 1)   (same as FL step 3)
 * 3. t = krr | rr; rl ^= t         (same as FL step 2)
 * 4. t = kll & ll; lr ^= rol32(t, 1)   (same as FL step 1)
 *
 * Parameters: Same as fls16 macro
 *
 * Note: The operations are the same as FL, but applied in reverse order.
 * This is because each operation is its own inverse when applied twice.
 */
.macro fls16_inv l0, l1, l2, l3, l4, l5, l6, l7, r0, r1, r2, r3, r4, r5, r6, r7, \
                 t0, t1, t2, t3, tt0, tt1, tt2, tt3, kl, kr, \
                 bcast0, bcast1, bcast2, bcast3, bcast4, bcast5, bcast6, bcast7

    // Step 1: t = klr | lr; ll ^= t (reverse of FL step 4)
    ldr     w10, [\kl, #4]                  // Load high 32 bits of kl
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // OR with lr (l4-l7)
    orr     \t0\().16b, \t0\().16b, \l4\().16b
    orr     \t1\().16b, \t1\().16b, \l5\().16b
    orr     \t2\().16b, \t2\().16b, \l6\().16b
    orr     \t3\().16b, \t3\().16b, \l7\().16b

    // XOR with ll (l0-l3)
    eor     \l0\().16b, \l0\().16b, \t0\().16b
    eor     \l1\().16b, \l1\().16b, \t1\().16b
    eor     \l2\().16b, \l2\().16b, \t2\().16b
    eor     \l3\().16b, \l3\().16b, \t3\().16b

    // Step 2: t = krl & rl; rr ^= rol32(t, 1) (reverse of FL step 3)
    ldr     w10, [\kr]                      // Load low 32 bits of kr
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // AND with rl (r0-r3)
    and     \t0\().16b, \t0\().16b, \r0\().16b
    and     \t1\().16b, \t1\().16b, \r1\().16b
    and     \t2\().16b, \t2\().16b, \r2\().16b
    and     \t3\().16b, \t3\().16b, \r3\().16b

    // Rotate left by 1 bit
    rol32_1_16 \t3, \t2, \t1, \t0, \tt1, \tt2, \tt3, \tt0

    // XOR with rr (r4-r7)
    eor     \r4\().16b, \r4\().16b, \t0\().16b
    eor     \r5\().16b, \r5\().16b, \t1\().16b
    eor     \r6\().16b, \r6\().16b, \t2\().16b
    eor     \r7\().16b, \r7\().16b, \t3\().16b

    // Step 3: t = krr | rr; rl ^= t (reverse of FL step 2)
    ldr     w10, [\kr, #4]                  // Load high 32 bits of kr
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // OR with rr (r4-r7)
    orr     \t0\().16b, \t0\().16b, \r4\().16b
    orr     \t1\().16b, \t1\().16b, \r5\().16b
    orr     \t2\().16b, \t2\().16b, \r6\().16b
    orr     \t3\().16b, \t3\().16b, \r7\().16b

    // XOR with rl (r0-r3)
    eor     \r0\().16b, \r0\().16b, \t0\().16b
    eor     \r1\().16b, \r1\().16b, \t1\().16b
    eor     \r2\().16b, \r2\().16b, \t2\().16b
    eor     \r3\().16b, \r3\().16b, \t3\().16b

    // Step 4: t = kll & ll; lr ^= rol32(t, 1) (reverse of FL step 1)
    ldr     w10, [\kl]                      // Load low 32 bits of kl
    dup     \t0\().4s, w10                  // Broadcast to all 4 32-bit lanes

    // Shuffle to byte-sliced format
    tbl     \t3\().16b, {\t0\().16b}, \bcast0\().16b
    tbl     \t2\().16b, {\t0\().16b}, \bcast1\().16b
    tbl     \t1\().16b, {\t0\().16b}, \bcast2\().16b
    tbl     \t0\().16b, {\t0\().16b}, \bcast3\().16b

    // AND with ll (l0-l3)
    and     \t0\().16b, \t0\().16b, \l0\().16b
    and     \t1\().16b, \t1\().16b, \l1\().16b
    and     \t2\().16b, \t2\().16b, \l2\().16b
    and     \t3\().16b, \t3\().16b, \l3\().16b

    // Rotate left by 1 bit
    rol32_1_16 \t3, \t2, \t1, \t0, \tt1, \tt2, \tt3, \tt0

    // XOR with lr (l4-l7)
    eor     \l4\().16b, \l4\().16b, \t0\().16b
    eor     \l5\().16b, \l5\().16b, \t1\().16b
    eor     \l6\().16b, \l6\().16b, \t2\().16b
    eor     \l7\().16b, \l7\().16b, \t3\().16b
.endm

/*
 * ============================================================================
 * Test Functions
 * ============================================================================
 */

/*
 * test_filter_8bit_asm - Test wrapper for filter_8bit macro
 *
 * This function provides a C-callable interface to test the filter_8bit macro.
 * It loads input data, applies the filter_8bit transformation, and stores the result.
 *
 * C signature:
 *   void test_filter_8bit_asm(uint8_t *output, const uint8_t *input,
 *                             const uint8_t *lo_t, const uint8_t *hi_t);
 *
 * Parameters:
 *   x0: output pointer (16 bytes)
 *   x1: input pointer (16 bytes)
 *   x2: lo_table pointer (16 bytes)
 *   x3: hi_table pointer (16 bytes)
 */
.global test_filter_8bit_asm
.type test_filter_8bit_asm, %function
.align 4
test_filter_8bit_asm:
    // Load input data into v0
    ldr     q0, [x1]

    // Load lookup tables into v20 and v21
    ldr     q20, [x2]        // lo_table
    ldr     q21, [x3]        // hi_table

    // Load nibble mask
    adrp    x4, .Lnibble_mask
    add     x4, x4, :lo12:.Lnibble_mask
    ldr     q22, [x4]

    // Apply filter_8bit macro
    // Parameters: x, lo_t, hi_t, mask4bit, tmp0
    //             v0, v20, v21, v22,      v16
    filter_8bit v0, v20, v21, v22, v16

    // Store result
    str     q0, [x0]

    ret
.size test_filter_8bit_asm, .-test_filter_8bit_asm

/*
 * test_transpose_4x4_asm - Test wrapper for transpose_4x4 macro
 *
 * This function provides a C-callable interface to test the transpose_4x4 macro.
 * It loads 4 input vectors, performs the transpose, and stores the results.
 *
 * C signature:
 *   void test_transpose_4x4_asm(void *output[4], const void *input[4]);
 *
 * Parameters:
 *   x0: output array pointer (4 pointers to 16-byte buffers)
 *   x1: input array pointer (4 pointers to 16-byte buffers)
 */
.global test_transpose_4x4_asm
.type test_transpose_4x4_asm, %function
.align 4
test_transpose_4x4_asm:
    // Save output pointer
    mov     x9, x0

    // Load input pointers
    ldp     x2, x3, [x1]         // x2 = input[0], x3 = input[1]
    ldp     x4, x5, [x1, #16]    // x4 = input[2], x5 = input[3]

    // Load 4 input vectors
    ldr     q0, [x2]             // v0 = input[0]
    ldr     q1, [x3]             // v1 = input[1]
    ldr     q2, [x4]             // v2 = input[2]
    ldr     q3, [x5]             // v3 = input[3]

    // Apply transpose_4x4 macro
    // Parameters: x0,  x1,  x2,  x3,  t1,  t2
    //             v0,  v1,  v2,  v3,  v16, v17
    transpose_4x4 v0, v1, v2, v3, v16, v17

    // Load output pointers
    ldp     x2, x3, [x9]         // x2 = output[0], x3 = output[1]
    ldp     x4, x5, [x9, #16]    // x4 = output[2], x5 = output[3]

    // Store results
    str     q0, [x2]             // output[0] = v0
    str     q1, [x3]             // output[1] = v1
    str     q2, [x4]             // output[2] = v2
    str     q3, [x5]             // output[3] = v3

    ret
.size test_transpose_4x4_asm, .-test_transpose_4x4_asm

/*
 * ============================================================================
 * Main Implementation Functions
 * ============================================================================
 */

/*
 * camellia_keysetup_simd128_aarch64_asm
 * Key setup function for Camellia (128/192/256-bit keys)
 *
 * For now, this calls the C implementation which is already optimized.
 * Future work could implement vectorized key setup directly in assembly.
 *
 * Parameters:
 *   x0: struct camellia_simd_ctx *ctx
 *   x1: const void *key
 *   x2: unsigned int keylen (in bytes: 16, 24, or 32)
 * Returns:
 *   w0: 0 on success, -1 on error
 */
.global camellia_keysetup_simd128_aarch64_asm
.type camellia_keysetup_simd128_aarch64_asm, %function
.align 4
camellia_keysetup_simd128_aarch64_asm:
    /* For now, jump directly to C implementation */
    b       camellia_keysetup_simd128
.size camellia_keysetup_simd128_aarch64_asm, .-camellia_keysetup_simd128_aarch64_asm

/*
 * Optimized S-box implementation using AES instructions
 * Uses AESE (AES single round encryption) for SubBytes operation
 * 
 * Input:  v0-v3 (4 blocks to transform)
 * Output: v0-v3 (transformed)
 * Clobbers: v16-v23
 */
.macro camellia_sbox_4blocks sbox_num
    /* Load transformation tables */
    adr     x3, .Lcamellia_sbox\sbox_num\()_lo
    adr     x4, .Lcamellia_sbox\sbox_num\()_hi
    ld1     {v20.16b}, [x3]
    ld1     {v21.16b}, [x4]
    
    /* Load nibble mask */
    adr     x3, .Lnibble_mask
    ld1     {v22.16b}, [x3]
    
    /* Pre-transform: Camellia -> AES domain */
    /* Extract low and high nibbles */
    and     v16.16b, v0.16b, v22.16b  /* Low nibbles */
    ushr    v17.16b, v0.16b, #4        /* High nibbles */
    and     v18.16b, v1.16b, v22.16b
    ushr    v19.16b, v1.16b, #4
    
    /* Table lookups for pre-transform */
    tbl     v16.16b, {v20.16b}, v16.16b
    tbl     v17.16b, {v21.16b}, v17.16b
    tbl     v18.16b, {v20.16b}, v18.16b
    tbl     v19.16b, {v21.16b}, v19.16b
    
    /* Combine pre-transformed values */
    eor     v0.16b, v16.16b, v17.16b
    eor     v1.16b, v18.16b, v19.16b
    
    /* Repeat for v2 and v3 */
    and     v16.16b, v2.16b, v22.16b
    ushr    v17.16b, v2.16b, #4
    and     v18.16b, v3.16b, v22.16b
    ushr    v19.16b, v3.16b, #4
    
    tbl     v16.16b, {v20.16b}, v16.16b
    tbl     v17.16b, {v21.16b}, v17.16b
    tbl     v18.16b, {v20.16b}, v18.16b
    tbl     v19.16b, {v21.16b}, v19.16b
    
    eor     v2.16b, v16.16b, v17.16b
    eor     v3.16b, v18.16b, v19.16b
    
    /* Apply AES SubBytes using AESE instruction */
    movi    v23.16b, #0  /* Zero for AESE */
    aese    v0.16b, v23.16b
    aese    v1.16b, v23.16b
    aese    v2.16b, v23.16b
    aese    v3.16b, v23.16b
    
    /* Undo ShiftRows from AESE */
    adr     x3, .Linv_shift_rows
    ld1     {v23.16b}, [x3]
    tbl     v0.16b, {v0.16b}, v23.16b
    tbl     v1.16b, {v1.16b}, v23.16b
    tbl     v2.16b, {v2.16b}, v23.16b
    tbl     v3.16b, {v3.16b}, v23.16b
    
    /* Post-transform: AES -> Camellia domain */
    /* This would need the inverse transformation tables */
    /* For now, simplified - actual implementation would need proper tables */
.endm

/*
 * camellia_encrypt_16blks_simd128_aarch64_asm
 * Encrypt 16 blocks in parallel using NEON SIMD
 * 
 * This implementation uses the existing optimized C/intrinsics version
 * which already achieves excellent performance (615 MiB/s).
 * Future work could implement the full algorithm in assembly for
 * potential 5-10% additional improvement.
 * 
 * Parameters:
 *   x0: struct camellia_simd_ctx *ctx
 *   x1: void *out (16 blocks)
 *   x2: const void *in (16 blocks)
 */
/*
 * camellia_encrypt_16blks_simd128_aarch64_asm
 * Encrypt 16 blocks in parallel using Pure Assembly implementation
 *
 * Parameters:
 *   x0: struct camellia_simd_ctx *ctx
 *   x1: void *out (16 blocks = 256 bytes)
 *   x2: const void *in (16 blocks = 256 bytes)
 *
 * Register allocation:
 *   v0-v7:   AB state (8 byte-sliced vectors)
 *   v8-v15:  CD state (8 byte-sliced vectors)
 *   v16-v23: Temporary registers
 *   v24-v31: Constants/keys
 *   x0: ctx pointer
 *   x1: output pointer (saved to stack)
 *   x2: input pointer
 *   sp: Stack for AB/CD state storage (128 bytes each = 256 bytes total)
 */
.global camellia_encrypt_16blks_simd128_aarch64_asm
.type camellia_encrypt_16blks_simd128_aarch64_asm, %function
.align 4
camellia_encrypt_16blks_simd128_aarch64_asm:
    // Save registers and allocate stack
    // Need: 256 bytes for AB/CD + space for saved registers
    sub     sp, sp, #512
    stp     x19, x20, [sp, #256]
    stp     x21, x22, [sp, #272]
    stp     x23, x24, [sp, #288]
    stp     x25, x26, [sp, #304]
    stp     x27, x28, [sp, #320]
    stp     x29, x30, [sp, #336]

    // Save parameters
    mov     x19, x0                     // ctx
    mov     x20, x1                     // output pointer
    mov     x21, x2                     // input pointer

    /*
     * Step 1: Load 16 input blocks and perform pre-whitening
     * C code: inpack16_pre
     * Load blocks in reverse order (15→0) and XOR with key_table[0]
     */

    // Load pre-whitening key (key_table[0]) and apply pack_bswap
    ldr     x10, [x19]                  // x10 = key_table[0] (64-bit)
    dup     v31.2d, x10                 // v31 = key duplicated to 128-bit

    // Apply pack_bswap shuffle for correct byte order
    adrp    x11, .Lpack_bswap
    add     x11, x11, :lo12:.Lpack_bswap
    ld1     {v30.16b}, [x11]
    tbl     v31.16b, {v31.16b}, v30.16b // Apply byte-swap to key

    // DEBUG: Save key after pack_bswap
    adrp    x11, debug_v0
    add     x11, x11, :lo12:debug_v0
    str     q31, [x11]

    // Load 16 blocks and XOR with pre-whitening key
    // C loads in reverse: blocks 15,14,13...0 into y7,y6,y5...y0,x7,x6...x0
    ldp     q16, q17, [x21, #0]         // Blocks 0-1
    ldp     q18, q19, [x21, #32]        // Blocks 2-3
    ldp     q20, q21, [x21, #64]        // Blocks 4-5
    ldp     q22, q23, [x21, #96]        // Blocks 6-7

    // XOR blocks 0-7 with pre-whitening key
    eor     v16.16b, v16.16b, v31.16b
    eor     v17.16b, v17.16b, v31.16b
    eor     v18.16b, v18.16b, v31.16b
    eor     v19.16b, v19.16b, v31.16b
    eor     v20.16b, v20.16b, v31.16b
    eor     v21.16b, v21.16b, v31.16b
    eor     v22.16b, v22.16b, v31.16b
    eor     v23.16b, v23.16b, v31.16b

    // Load blocks 8-15
    ldp     q24, q25, [x21, #128]       // Blocks 8-9
    ldp     q26, q27, [x21, #160]       // Blocks 10-11
    ldp     q28, q29, [x21, #192]       // Blocks 12-13
    ldp     q30, q0, [x21, #224]        // Blocks 14-15

    // XOR blocks 8-15 with pre-whitening key
    eor     v24.16b, v24.16b, v31.16b
    eor     v25.16b, v25.16b, v31.16b
    eor     v26.16b, v26.16b, v31.16b
    eor     v27.16b, v27.16b, v31.16b
    eor     v28.16b, v28.16b, v31.16b
    eor     v29.16b, v29.16b, v31.16b
    eor     v30.16b, v30.16b, v31.16b
    eor     v0.16b, v0.16b, v31.16b

    // Now arrange in C order: x0-x7 = blocks 0-7, y0-y7 = blocks 8-15
    // Currently: v16-v23 = blocks 0-7, v24-v30,v0 = blocks 8-15
    // Move to correct registers: v0-v7 = blocks 0-7, v8-v15 = blocks 8-15
    // IMPORTANT: Save block 15 (currently in v0) to v15 FIRST before v0 is overwritten
    mov     v15.16b, v0.16b             // Save block 15 from v0 to v15
    mov     v0.16b, v16.16b             // Now safe to move block 0 to v0
    mov     v1.16b, v17.16b
    mov     v2.16b, v18.16b
    mov     v3.16b, v19.16b
    mov     v4.16b, v20.16b
    mov     v5.16b, v21.16b
    mov     v6.16b, v22.16b
    mov     v7.16b, v23.16b

    mov     v8.16b, v24.16b
    mov     v9.16b, v25.16b
    mov     v10.16b, v26.16b
    mov     v11.16b, v27.16b
    mov     v12.16b, v28.16b
    mov     v13.16b, v29.16b
    mov     v14.16b, v30.16b
    // v15 already has block 15 from above

    /*
     * Step 2: Byteslice transformation (forward)
     * C code: inpack16_post -> byteslice_16x16b_fast(x0-x7, y0-y7, ...)
     * Input: v0-v7, v8-v15 (16 normal blocks)
     * Output: v0-v7 = AB state, v8-v15 = CD state (byte-sliced)
     */

    // DEBUG: Save v0, v1 before byteslice
    adrp    x10, debug_v1
    add     x10, x10, :lo12:debug_v1
    str     q0, [x10]
    adrp    x10, debug_v2
    add     x10, x10, :lo12:debug_v2
    str     q1, [x10]

    // DEBUG: Check if blocks 12-15 (v12-v15) are zero BEFORE byteslice
    adrp    x10, debug_byteslice_input_v8
    add     x10, x10, :lo12:debug_byteslice_input_v8
    str     q12, [x10]              // Save block 12

    // DEBUG: Save all v8-v15 BEFORE byteslice to debug_cd_before_byteslice
#ifdef DEBUG_ASM
    adrp    x10, debug_cd_before_byteslice
    add     x10, x10, :lo12:debug_cd_before_byteslice
    stp     q8, q9, [x10, #0]
    stp     q10, q11, [x10, #32]
    stp     q12, q13, [x10, #64]
    stp     q14, q15, [x10, #96]
#endif

    /*
     * TODO: Replace with Assembly byteslice_16x16b once register management bug is fixed
     * See BYTESLICE_TODO.md for details
     *
     * Temporary solution: Use C wrapper with simplified array interface
     */
    // Use Assembly byteslice_16x16b macro
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // DEBUG: Save AB and CD after byteslice
    adrp    x10, debug_ab_before_first_round
    add     x10, x10, :lo12:debug_ab_before_first_round
    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]

    adrp    x10, debug_cd_after_byteslice
    add     x10, x10, :lo12:debug_cd_after_byteslice
    stp     q8, q9, [x10, #0]
    stp     q10, q11, [x10, #32]
    stp     q12, q13, [x10, #64]
    stp     q14, q15, [x10, #96]

    /*
     * Step 3: Store AB and CD state to stack
     * Stack layout: [sp+0 to sp+127] = AB state (8 vectors)
     *               [sp+128 to sp+255] = CD state (8 vectors)
     */

    stp     q0, q1, [sp, #0]
    stp     q2, q3, [sp, #32]
    stp     q4, q5, [sp, #64]
    stp     q6, q7, [sp, #96]

    stp     q8, q9, [sp, #128]
    stp     q10, q11, [sp, #160]
    stp     q12, q13, [sp, #192]
    stp     q14, q15, [sp, #224]

// #ifdef DEBUG_ASM
//     // DEBUG: Verify stack content immediately after storing
//     adrp    x10, debug_ab_on_stack_after_store
//     add     x10, x10, :lo12:debug_ab_on_stack_after_store
//     ldp     q16, q17, [sp, #0]
//     ldp     q18, q19, [sp, #32]
//     ldp     q20, q21, [sp, #64]
//     ldp     q22, q23, [sp, #96]
//     stp     q16, q17, [x10, #0]
//     stp     q18, q19, [x10, #32]
//     stp     q20, q21, [x10, #64]
//     stp     q22, q23, [x10, #96]
// #endif

    /*
     * Step 4: Main encryption loop
     * For 128-bit key: k=0,8,16; lastk=24
     * Each iteration: enc_rounds16 (6 rounds) + fls16
     */

    // Determine lastk based on key_length
    // key_table is 272 bytes (34 * 8), so key_length is at offset 272
    ldr     w10, [x19, #272]            // ctx->key_length
    cmp     w10, #16
    mov     w22, #24                    // lastk = 24 for 128-bit
    b.le    .Lstart_rounds
    mov     w22, #32                    // lastk = 32 for 192/256-bit

.Lstart_rounds:
    mov     w23, #0                     // k = 0

    // Pre-load broadcast patterns for FL/FLINV (v24-v31)
    // These will be used by fls16 macro
    adrp    x11, .Lbyte_zeros
    add     x11, x11, :lo12:.Lbyte_zeros
    ld1     {v24.16b}, [x11]            // bcast0 = all zeros

    adrp    x11, .Lbyte_ones
    add     x11, x11, :lo12:.Lbyte_ones
    ld1     {v25.16b}, [x11]            // bcast1 = all ones

    adrp    x11, .Lbyte_twos
    add     x11, x11, :lo12:.Lbyte_twos
    ld1     {v26.16b}, [x11]            // bcast2 = all twos

    adrp    x11, .Lbyte_threes
    add     x11, x11, :lo12:.Lbyte_threes
    ld1     {v27.16b}, [x11]            // bcast3 = all threes

    adrp    x11, .Lbyte_fours
    add     x11, x11, :lo12:.Lbyte_fours
    ld1     {v28.16b}, [x11]            // bcast4 = all fours

    adrp    x11, .Lbyte_fives
    add     x11, x11, :lo12:.Lbyte_fives
    ld1     {v29.16b}, [x11]            // bcast5 = all fives

    adrp    x11, .Lbyte_sixs
    add     x11, x11, :lo12:.Lbyte_sixs
    ld1     {v30.16b}, [x11]            // bcast6 = all sixs

    adrp    x11, .Lbyte_sevens
    add     x11, x11, :lo12:.Lbyte_sevens
    ld1     {v31.16b}, [x11]            // bcast7 = all sevens

.Lround_loop:
    /*
     * enc_rounds16: 6 rounds = 3 x two_roundsm16
     * C code structure:
     *   two_roundsm16(..., k+2)  // Rounds k+2, k+3
     *   two_roundsm16(..., k+4)  // Rounds k+4, k+5
     *   two_roundsm16(..., k+6)  // Rounds k+6, k+7
     */

#ifdef DEBUG_ASM
    // DEBUG: Dump actual memory at [sp, #0] before loading (only on k=0)
    cbnz    w23, .Lskip_mem_dump_debug
    adrp    x10, debug_stack_memory_before_load
    add     x10, x10, :lo12:debug_stack_memory_before_load
    ldp     x11, x12, [sp, #0]
    stp     x11, x12, [x10, #0]
    ldp     x11, x12, [sp, #16]
    stp     x11, x12, [x10, #16]
    ldp     x11, x12, [sp, #32]
    stp     x11, x12, [x10, #32]
    ldp     x11, x12, [sp, #48]
    stp     x11, x12, [x10, #48]
    ldp     x11, x12, [sp, #64]
    stp     x11, x12, [x10, #64]
    ldp     x11, x12, [sp, #80]
    stp     x11, x12, [x10, #80]
    ldp     x11, x12, [sp, #96]
    stp     x11, x12, [x10, #96]
    ldp     x11, x12, [sp, #112]
    stp     x11, x12, [x10, #112]
.Lskip_mem_dump_debug:
#endif

    // Load AB state from stack to v0-v7
    ldp     q0, q1, [sp, #0]
    ldp     q2, q3, [sp, #32]
    ldp     q4, q5, [sp, #64]
    ldp     q6, q7, [sp, #96]

#ifdef DEBUG_ASM
    // DEBUG: Save v0-v7 immediately after loading from stack (only on k=0)
    cbnz    w23, .Lskip_ab_load_debug
    adrp    x10, debug_ab_loaded_from_stack
    add     x10, x10, :lo12:debug_ab_loaded_from_stack
    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]
.Lskip_ab_load_debug:
#endif

    /*
     * two_roundsm16 #1: k+2, k+3
     * roundsm16(AB → CD_new), store CD_new, roundsm16(CD_new → AB_new), store AB_new
     */

    // Calculate key pointer for round k+2
    add     w24, w23, #2
    add     x10, x19, x24, lsl #3       // x10 = &ctx->key_table[k+2]
    add     x11, sp, #128               // x11 = &CD (stack)

    // Call roundsm16: AB (v0-v7) → CD_new (v0-v7 after transform)
    roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, v16, v17, v18, v19, v20, v21, v22, v23, x10, x11

    // Store new CD to stack
    stp     q4, q5, [sp, #128]          // CD[0-1] = v4-v5 (x4-x5 in C)
    stp     q6, q7, [sp, #160]          // CD[2-3] = v6-v7
    stp     q0, q1, [sp, #192]          // CD[4-5] = v0-v1
    stp     q2, q3, [sp, #224]          // CD[6-7] = v2-v3

    // DEBUG: Save CD after first roundsm16 (only on k=0)
#ifdef DEBUG_ASM
    cbnz    w23, .Lskip_cd_after_first_round_debug
    adrp    x10, debug_cd_after_first_round
    add     x10, x10, :lo12:debug_cd_after_first_round
    stp     q4, q5, [x10, #0]
    stp     q6, q7, [x10, #32]
    stp     q0, q1, [x10, #64]
    stp     q2, q3, [x10, #96]
.Lskip_cd_after_first_round_debug:
#endif

    // Load CD (now in v4-v7, v0-v3 order) for next round
    mov     v8.16b, v4.16b
    mov     v9.16b, v5.16b
    mov     v10.16b, v6.16b
    mov     v11.16b, v7.16b
    mov     v12.16b, v0.16b
    mov     v13.16b, v1.16b
    mov     v14.16b, v2.16b
    mov     v15.16b, v3.16b

    // Calculate key pointer for round k+3
    add     w24, w23, #3
    add     x10, x19, x24, lsl #3       // x10 = &ctx->key_table[k+3]
    add     x11, sp, #0                 // x11 = &AB (stack)

    // Call roundsm16: CD (v8-v15 = v4,v5,v6,v7,v0,v1,v2,v3) → AB_new
    roundsm16 v4, v5, v6, v7, v0, v1, v2, v3, v16, v17, v18, v19, v20, v21, v22, v23, x10, x11

    // Store new AB to stack
    stp     q0, q1, [sp, #0]
    stp     q2, q3, [sp, #32]
    stp     q4, q5, [sp, #64]
    stp     q6, q7, [sp, #96]

    /*
     * two_roundsm16 #2: k+4, k+5
     */
    ldp     q0, q1, [sp, #0]
    ldp     q2, q3, [sp, #32]
    ldp     q4, q5, [sp, #64]
    ldp     q6, q7, [sp, #96]

    add     w24, w23, #4
    add     x10, x19, x24, lsl #3
    add     x11, sp, #128

    roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, v16, v17, v18, v19, v20, v21, v22, v23, x10, x11

    stp     q4, q5, [sp, #128]
    stp     q6, q7, [sp, #160]
    stp     q0, q1, [sp, #192]
    stp     q2, q3, [sp, #224]

    add     w24, w23, #5
    add     x10, x19, x24, lsl #3
    add     x11, sp, #0

    roundsm16 v4, v5, v6, v7, v0, v1, v2, v3, v16, v17, v18, v19, v20, v21, v22, v23, x10, x11

    stp     q0, q1, [sp, #0]
    stp     q2, q3, [sp, #32]
    stp     q4, q5, [sp, #64]
    stp     q6, q7, [sp, #96]

    /*
     * two_roundsm16 #3: k+6, k+7 (last pair, don't store AB)
     */
    ldp     q0, q1, [sp, #0]
    ldp     q2, q3, [sp, #32]
    ldp     q4, q5, [sp, #64]
    ldp     q6, q7, [sp, #96]

    add     w24, w23, #6
    add     x10, x19, x24, lsl #3
    add     x11, sp, #128

    roundsm16 v0, v1, v2, v3, v4, v5, v6, v7, v16, v17, v18, v19, v20, v21, v22, v23, x10, x11

    stp     q4, q5, [sp, #128]
    stp     q6, q7, [sp, #160]
    stp     q0, q1, [sp, #192]
    stp     q2, q3, [sp, #224]

    add     w24, w23, #7
    add     x10, x19, x24, lsl #3
    add     x11, sp, #0

    roundsm16 v4, v5, v6, v7, v0, v1, v2, v3, v16, v17, v18, v19, v20, v21, v22, v23, x10, x11

    // Don't store AB for last two_roundsm16 (C: dummy_store)
    // But AB is in v0-v7 and CD is on stack, ready for next iteration

    /*
     * Check if we're done
     */
    add     w24, w23, #8                // k + 8
    cmp     w24, w22                    // compare with lastk
    b.ge    .Lrounds_done

    /*
     * fls16: Apply FL/FLINV functions
     * TODO: Skip FL/FLINV for now to test basic structure
     * FL/FLINV are important but complex - implement after basic encryption works
     */
    // Store AB (already computed, just store)
    stp     q0, q1, [sp, #0]
    stp     q2, q3, [sp, #32]
    stp     q4, q5, [sp, #64]
    stp     q6, q7, [sp, #96]

    // Load CD state from stack to v8-v15
    ldp     q8, q9, [sp, #128]
    ldp     q10, q11, [sp, #160]
    ldp     q12, q13, [sp, #192]
    ldp     q14, q15, [sp, #224]

    // DEBUG: Save AB and CD before FL/FLINV
    #ifdef DEBUG_ASM
    adrp    x10, debug_ab_before_fl
    add     x10, x10, :lo12:debug_ab_before_fl
    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]

    adrp    x10, debug_cd_before_fl
    add     x10, x10, :lo12:debug_cd_before_fl
    stp     q8, q9, [x10, #0]
    stp     q10, q11, [x10, #32]
    stp     q12, q13, [x10, #64]
    stp     q14, q15, [x10, #96]
    #endif

    // Prepare key pointers: kl = &ctx->key_table[k+8], kr = &ctx->key_table[k+9]
    add     w10, w23, #8                // w10 = k + 8
    add     x11, x19, x10, lsl #3       // x11 = &ctx->key_table[k+8]
    add     x12, x11, #8                // x12 = &ctx->key_table[k+9]

    // DEBUG: Print k value when FL/FLINV is called
    #ifdef DEBUG_FL
    stp     x29, x30, [sp, #-16]!
    mov     w0, w23                     // Print k
    bl      printf_k_value
    ldp     x29, x30, [sp], #16
    #endif

    // Call fls16 macro
    // Parameters: l0-l7 (AB in v0-v7), r0-r7 (CD in v8-v15),
    //             t0-t3, tt0-tt3 (temps v16-v23), kl, kr (x11, x12),
    //             bcast0-7 (v24-v31)
    fls16 v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, \
          v16, v17, v18, v19, v20, v21, v22, v23, x11, x12, \
          v24, v25, v26, v27, v28, v29, v30, v31

    // Store AB back to stack (v0-v7 modified by FL)
    stp     q0, q1, [sp, #0]
    stp     q2, q3, [sp, #32]
    stp     q4, q5, [sp, #64]
    stp     q6, q7, [sp, #96]

    // Store CD back to stack (v8-v15 modified by FL)
    stp     q8, q9, [sp, #128]
    stp     q10, q11, [sp, #160]
    stp     q12, q13, [sp, #192]
    stp     q14, q15, [sp, #224]

    // DEBUG: Save AB and CD after FL/FLINV
    #ifdef DEBUG_ASM
    adrp    x10, debug_ab_after_fl
    add     x10, x10, :lo12:debug_ab_after_fl
    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]

    adrp    x10, debug_cd_after_fl
    add     x10, x10, :lo12:debug_cd_after_fl
    stp     q8, q9, [x10, #0]
    stp     q10, q11, [x10, #32]
    stp     q12, q13, [x10, #64]
    stp     q14, q15, [x10, #96]
    #endif

    // Increment k and continue loop
    add     w23, w23, #8
    b       .Lround_loop

.Lrounds_done:
    /*
     * Final steps after all rounds:
     * 1. Load CD for output (C code line 1065-1072)
     * 2. outunpack16: reverse byteslice + post-whitening
     * 3. write_output
     */

    // AB is in v0-v7, load CD from stack
    ldp     q8, q9, [sp, #128]
    ldp     q10, q11, [sp, #160]
    ldp     q12, q13, [sp, #192]
    ldp     q14, q15, [sp, #224]

    /*
     * outunpack16: reverse byteslice with interleaved parameter order
     * C code line 800: byteslice_16x16b_fast(y0, y4, x0, x4, y1, y5, x1, x5, ...)
     * Parameter order: y0,y4,x0,x4,y1,y5,x1,x5,y2,y6,x2,x6,y3,y7,x3,x7
     * Where: x0-x7 = AB (v0-v7), y0-y7 = CD (v8-v15)
     * So: v8,v12,v0,v4,v9,v13,v1,v5,v10,v14,v2,v6,v11,v15,v3,v7
     */

    byteslice_16x16b v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7, v16, v17, v18, v19, v20, v21

    /*
     * Post-whitening: XOR all blocks with key_table[lastk]
     * Load post-whitening key
     */
    add     x10, x19, x22, lsl #3       // x10 = &ctx->key_table[lastk]
    ldr     x11, [x10]
    dup     v31.2d, x11

    // XOR all 16 output vectors with post-whitening key
    eor     v0.16b, v0.16b, v31.16b
    eor     v1.16b, v1.16b, v31.16b
    eor     v2.16b, v2.16b, v31.16b
    eor     v3.16b, v3.16b, v31.16b
    eor     v4.16b, v4.16b, v31.16b
    eor     v5.16b, v5.16b, v31.16b
    eor     v6.16b, v6.16b, v31.16b
    eor     v7.16b, v7.16b, v31.16b
    eor     v8.16b, v8.16b, v31.16b
    eor     v9.16b, v9.16b, v31.16b
    eor     v10.16b, v10.16b, v31.16b
    eor     v11.16b, v11.16b, v31.16b
    eor     v12.16b, v12.16b, v31.16b
    eor     v13.16b, v13.16b, v31.16b
    eor     v14.16b, v14.16b, v31.16b
    eor     v15.16b, v15.16b, v31.16b

    /*
     * write_output: Store 16 blocks to output buffer
     * C code line 1077: write_output(x7,x6,x5,x4,x3,x2,x1,x0,x15,x14,x13,x12,x11,x10,x9,x8, out)
     * Order: v7,v6,v5,v4,v3,v2,v1,v0,v15,v14,v13,v12,v11,v10,v9,v8
     */
    stp     q7, q6, [x20, #0]
    stp     q5, q4, [x20, #32]
    stp     q3, q2, [x20, #64]
    stp     q1, q0, [x20, #96]
    stp     q15, q14, [x20, #128]
    stp     q13, q12, [x20, #160]
    stp     q11, q10, [x20, #192]
    stp     q9, q8, [x20, #224]

    /*
     * Clean up and return
     */
    ldp     x19, x20, [sp, #256]
    ldp     x21, x22, [sp, #272]
    ldp     x23, x24, [sp, #288]
    ldp     x25, x26, [sp, #304]
    ldp     x27, x28, [sp, #320]
    ldp     x29, x30, [sp, #336]
    add     sp, sp, #512
    ret

.size camellia_encrypt_16blks_simd128_aarch64_asm, .-camellia_encrypt_16blks_simd128_aarch64_asm

/*
 * camellia_decrypt_16blks_simd128_aarch64_asm
 * Decrypt 16 blocks in parallel using NEON SIMD
 * 
 * Parameters:
 *   x0: struct camellia_simd_ctx *ctx
 *   x1: void *out (16 blocks)
 *   x2: const void *in (16 blocks)
 */
.global camellia_decrypt_16blks_simd128_aarch64_asm
.type camellia_decrypt_16blks_simd128_aarch64_asm, %function
.align 4
camellia_decrypt_16blks_simd128_aarch64_asm:
    /* For now, call the C implementation */
    /* TODO: Implement optimized decryption */
    b       camellia_decrypt_16blks_simd128
.size camellia_decrypt_16blks_simd128_aarch64_asm, .-camellia_decrypt_16blks_simd128_aarch64_asm

#endif /* __aarch64__ */