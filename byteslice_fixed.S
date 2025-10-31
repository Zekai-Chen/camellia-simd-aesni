/*
 * Fixed byteslice - uses stack memory for st0, st1 like C code
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
 * byteslice using stack memory for st0, st1
 * Stack layout: [sp+0] = st0, [sp+16] = st1
 */
.macro byteslice_16x16b_fixed a0, b0, c0, d0, a1, b1, c1, d1, \
                               a2, b2, c2, d2, a3, b3, c3, d3, \
                               t0, t1, t2, t3, t4, t5

    // Allocate stack space for st0, st1 (32 bytes)
    sub     sp, sp, #32

    // C line 709-710: Save original d2, d3 to stack
    str     \d2, [sp, #0]           // st0 = d2 (原始block 11)
    str     \d3, [sp, #16]          // st1 = d3 (原始block 15)

    // C line 711-712: Transpose groups 0-1
    transpose_4x4 \a0, \a1, \a2, \a3, \d2, \d3
    transpose_4x4 \b0, \b1, \b2, \b3, \d2, \d3

    // C line 713-714: Restore d2, d3 from stack
    ldr     \d2, [sp, #0]           // d2 = st0
    ldr     \d3, [sp, #16]          // d3 = st1

    // C line 716-717: Load into a0, a1
    ldr     \a0, [sp, #0]           // a0 = st0
    ldr     \a1, [sp, #16]          // a1 = st1

    // C line 718-719: Transpose groups 2-3
    transpose_4x4 \c0, \c1, \c2, \c3, \a0, \a1
    transpose_4x4 \d0, \d1, \d2, \d3, \a0, \a1

    // C line 721: Load shuffle pattern
    adrp    x10, .Lshufb_16x16b
    add     x10, x10, :lo12:.Lshufb_16x16b
    ld1     {\a0\().16b}, [x10]

    // C line 722: Load st1 into a1
    ldr     \a1, [sp, #16]          // a1 = st1

    // C lines 723-737: Shuffle 15 vectors
    tbl     \a2\().16b, {\a2\().16b}, \a0\().16b
    tbl     \a3\().16b, {\a3\().16b}, \a0\().16b
    tbl     \b0\().16b, {\b0\().16b}, \a0\().16b
    tbl     \b1\().16b, {\b1\().16b}, \a0\().16b
    tbl     \b2\().16b, {\b2\().16b}, \a0\().16b
    tbl     \b3\().16b, {\b3\().16b}, \a0\().16b
    tbl     \a1\().16b, {\a1\().16b}, \a0\().16b
    tbl     \c0\().16b, {\c0\().16b}, \a0\().16b
    tbl     \c1\().16b, {\c1\().16b}, \a0\().16b
    tbl     \c2\().16b, {\c2\().16b}, \a0\().16b
    tbl     \c3\().16b, {\c3\().16b}, \a0\().16b
    tbl     \d0\().16b, {\d0\().16b}, \a0\().16b
    tbl     \d1\().16b, {\d1\().16b}, \a0\().16b
    tbl     \d2\().16b, {\d2\().16b}, \a0\().16b
    tbl     \d3\().16b, {\d3\().16b}, \a0\().16b

    // C line 738: Save shuffled d3 to st1
    str     \d3, [sp, #16]          // st1 = d3

    // C line 739: Load st0 into d3
    ldr     \d3, [sp, #0]           // d3 = st0

    // C line 740: Shuffle d3 into a0
    tbl     \a0\().16b, {\d3\().16b}, \a0\().16b

    // C line 741: Load st0 into d2
    ldr     \d2, [sp, #0]           // d2 = st0

    // C line 743-744: Transpose Phase 3 groups 0-1
    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3

    // C line 745-746: Save temps to stack
    str     \d2, [sp, #0]           // st0 = d2
    str     \d3, [sp, #16]          // st1 = d3

    // C line 748-749: Load temps into b0, b1
    ldr     \b0, [sp, #0]           // b0 = st0
    ldr     \b1, [sp, #16]          // b1 = st1

    // C line 750-751: Transpose Phase 3 groups 2-3
    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1

    // C line 752-753: Save final b0, b1 to stack
    str     \b0, [sp, #0]           // st0 = b0
    str     \b1, [sp, #16]          // st1 = b1

    // Restore stack
    add     sp, sp, #32

.endm

.globl test_byteslice_16x16b_asm
.type test_byteslice_16x16b_asm, %function
test_byteslice_16x16b_asm:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    mov     x10, x0

    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]
    ldp     q4, q5, [x1, #64]
    ldp     q6, q7, [x1, #96]
    ldp     q8, q9, [x1, #128]
    ldp     q10, q11, [x1, #160]
    ldp     q12, q13, [x1, #192]
    ldp     q14, q15, [x1, #224]

    byteslice_16x16b_fixed v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]
    stp     q8, q9, [x10, #128]
    stp     q10, q11, [x10, #160]
    stp     q12, q13, [x10, #192]
    stp     q14, q15, [x10, #224]

    ldp     x29, x30, [sp], #16
    ret
.size test_byteslice_16x16b_asm, .-test_byteslice_16x16b_asm
