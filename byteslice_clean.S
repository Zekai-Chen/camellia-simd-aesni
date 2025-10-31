/*
 * Clean byteslice - mechanical C to Assembly translation
 * C code from camellia_simd128_with_aes_instruction_set.c lines 707-754
 */
.data
.align 4

.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15

.text
.align 4

.macro transpose_4x4 x0, x1, x2, x3, t1, t2
    zip2    \t2\().4s, \x0\().4s, \x1\().4s
    zip1    \x0\().4s, \x0\().4s, \x1\().4s
    zip1    \t1\().4s, \x2\().4s, \x3\().4s
    zip2    \x2\().4s, \x2\().4s, \x3\().4s
    zip2    \x1\().2d, \x0\().2d, \t1\().2d
    zip1    \x0\().2d, \x0\().2d, \t1\().2d
    zip2    \x3\().2d, \t2\().2d, \x2\().2d
    zip1    \x2\().2d, \t2\().2d, \x2\().2d
.endm

/*
 * Direct C-to-Assembly mapping:
 * C st0 → Assembly t4
 * C st1 → Assembly t5
 * C shufb_16x16b_stack → Assembly loaded from .Lshufb_16x16b
 */
.macro byteslice_16x16b_clean a0, b0, c0, d0, a1, b1, c1, d1, \
                               a2, b2, c2, d2, a3, b3, c3, d3, \
                               t0, t1, t2, t3, t4, t5

    // C line 709-710: SAVE original d2, d3 BEFORE transpose
    // vmovdqa128(d2, st0) → st0 = d2
    // vmovdqa128(d3, st1) → st1 = d3
    mov     \t4\().16b, \d2\().16b      // t4 = original d2 (block 11)
    mov     \t5\().16b, \d3\().16b      // t5 = original d3 (block 15)

    // C line 711-712: Transpose groups 0-1 (corrupts d2, d3)
    transpose_4x4 \a0, \a1, \a2, \a3, \d2, \d3
    transpose_4x4 \b0, \b1, \b2, \b3, \d2, \d3

    // C line 713-714: RESTORE original d2, d3
    // vmovdqa128(st0, d2) → d2 = st0
    // vmovdqa128(st1, d3) → d3 = st1
    mov     \d2\().16b, \t4\().16b      // d2 = original d2 (restored!)
    mov     \d3\().16b, \t5\().16b      // d3 = original d3 (restored!)

    // C line 716-717: Load into a0, a1 for use as temps
    // vmovdqa128(a0, st0) → a0 = st0
    // vmovdqa128(a1, st1) → a1 = st1
    mov     \a0\().16b, \t4\().16b      // a0 = original d2
    mov     \a1\().16b, \t5\().16b      // a1 = original d3

    // C line 718-719: Transpose groups 2-3 using a0, a1 as temps
    transpose_4x4 \c0, \c1, \c2, \c3, \a0, \a1
    transpose_4x4 \d0, \d1, \d2, \d3, \a0, \a1

    // C line 721: shufb_16x16b_stack = a0 (WRONG! It's a0 = shufb...)
    // Actually: vmovdqa128(shufb_16x16b_stack, a0) means a0 = shufb_16x16b_stack
    adrp    x10, .Lshufb_16x16b
    add     x10, x10, :lo12:.Lshufb_16x16b
    ld1     {\t2\().16b}, [x10]
    mov     \a0\().16b, \t2\().16b

    // C line 722: st1 = a1
    mov     \t5\().16b, \a1\().16b

    // C lines 723-737: shuffle using a0 as pattern
    tbl     \a2\().16b, {\a2\().16b}, \a0\().16b
    tbl     \a3\().16b, {\a3\().16b}, \a0\().16b
    tbl     \b0\().16b, {\b0\().16b}, \a0\().16b
    tbl     \b1\().16b, {\b1\().16b}, \a0\().16b
    tbl     \b2\().16b, {\b2\().16b}, \a0\().16b
    tbl     \b3\().16b, {\b3\().16b}, \a0\().16b
    tbl     \a1\().16b, {\t5\().16b}, \a0\().16b    // shuffle saved a1 from t5
    tbl     \c0\().16b, {\c0\().16b}, \a0\().16b
    tbl     \c1\().16b, {\c1\().16b}, \a0\().16b
    tbl     \c2\().16b, {\c2\().16b}, \a0\().16b
    tbl     \c3\().16b, {\c3\().16b}, \a0\().16b
    tbl     \d0\().16b, {\d0\().16b}, \a0\().16b
    tbl     \d1\().16b, {\d1\().16b}, \a0\().16b
    tbl     \d2\().16b, {\d2\().16b}, \a0\().16b
    tbl     \d3\().16b, {\d3\().16b}, \a0\().16b

    // C line 738: d3 = st1 (WRONG! It's st1 = d3)
    // vmovdqa128(d3, st1) means st1 = d3
    mov     \t5\().16b, \d3\().16b

    // C line 739: vmovdqa128(st0, d3) means d3 = st0
    mov     \d3\().16b, \t4\().16b

    // C line 740: vpshufb128(a0, d3, a0) means a0 = shuffle(d3, a0)
    tbl     \a0\().16b, {\d3\().16b}, \a0\().16b

    // C line 741: vmovdqa128(d2, st0) means d2 = st0
    mov     \d2\().16b, \t4\().16b

    // C line 743-744
    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3

    // C line 745-746: st0 = d2; st1 = d3
    mov     \t4\().16b, \d2\().16b
    mov     \t5\().16b, \d3\().16b

    // C line 748-749: b0 = st0; b1 = st1
    mov     \b0\().16b, \t4\().16b
    mov     \b1\().16b, \t5\().16b

    // C line 750-751
    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1

    // C line 752-753: st0 = b0; st1 = b1
    mov     \t4\().16b, \b0\().16b
    mov     \t5\().16b, \b1\().16b

.endm

.globl test_byteslice_16x16b_asm
.type test_byteslice_16x16b_asm, %function
test_byteslice_16x16b_asm:
    mov     x10, x0

    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]
    ldp     q4, q5, [x1, #64]
    ldp     q6, q7, [x1, #96]
    ldp     q8, q9, [x1, #128]
    ldp     q10, q11, [x1, #160]
    ldp     q12, q13, [x1, #192]
    ldp     q14, q15, [x1, #224]

    byteslice_16x16b_clean v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]
    stp     q8, q9, [x10, #128]
    stp     q10, q11, [x10, #160]
    stp     q12, q13, [x10, #192]
    stp     q14, q15, [x10, #224]

    ret
.size test_byteslice_16x16b_asm, .-test_byteslice_16x16b_asm
