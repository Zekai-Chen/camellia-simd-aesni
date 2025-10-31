/* Test byteslice roundtrip */
.text

// Forward declare the macros (they're in the main file)
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

.macro byteslice_16x16b a0, b0, c0, d0, a1, b1, c1, d1, \
                        a2, b2, c2, d2, a3, b3, c3, d3, \
                        t0, t1, t2, t3, t4, t5

    // Phase 1: First level of transpose
    mov     \t4\().16b, \d2\().16b
    mov     \t5\().16b, \d3\().16b

    transpose_4x4 \a0, \a1, \a2, \a3, \d2, \d3
    transpose_4x4 \b0, \b1, \b2, \b3, \d2, \d3

    mov     \d2\().16b, \t4\().16b
    mov     \d3\().16b, \t5\().16b
    mov     \t4\().16b, \a0\().16b
    mov     \t5\().16b, \a1\().16b

    transpose_4x4 \c0, \c1, \c2, \c3, \a0, \a1
    transpose_4x4 \d0, \d1, \d2, \d3, \a0, \a1

    // Phase 2: Shuffle bytes within vectors
    adrp    x10, .Lshufb_16x16b
    add     x10, x10, :lo12:.Lshufb_16x16b
    ld1     {\t2\().16b}, [x10]

    mov     \t3\().16b, \t4\().16b
    mov     \t0\().16b, \t5\().16b

    tbl     \a0\().16b, {\t3\().16b}, \t2\().16b
    tbl     \a1\().16b, {\t0\().16b}, \t2\().16b
    tbl     \a2\().16b, {\a2\().16b}, \t2\().16b
    tbl     \a3\().16b, {\a3\().16b}, \t2\().16b
    tbl     \b0\().16b, {\b0\().16b}, \t2\().16b
    tbl     \b1\().16b, {\b1\().16b}, \t2\().16b
    tbl     \b2\().16b, {\b2\().16b}, \t2\().16b
    tbl     \b3\().16b, {\b3\().16b}, \t2\().16b
    tbl     \c0\().16b, {\c0\().16b}, \t2\().16b
    tbl     \c1\().16b, {\c1\().16b}, \t2\().16b
    tbl     \c2\().16b, {\c2\().16b}, \t2\().16b
    tbl     \c3\().16b, {\c3\().16b}, \t2\().16b
    tbl     \d0\().16b, {\d0\().16b}, \t2\().16b
    tbl     \d1\().16b, {\d1\().16b}, \t2\().16b
    tbl     \d2\().16b, {\d2\().16b}, \t2\().16b
    tbl     \d3\().16b, {\d3\().16b}, \t2\().16b

    // Phase 3: Second level of transpose
    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3

    mov     \t4\().16b, \d2\().16b
    mov     \t5\().16b, \d3\().16b

    mov     \t0\().16b, \b0\().16b
    mov     \t1\().16b, \b1\().16b
    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1

    mov     \b0\().16b, \t0\().16b
    mov     \b1\().16b, \t1\().16b

    mov     \d2\().16b, \t4\().16b
    mov     \d3\().16b, \t5\().16b

.endm

.section .rodata
.align 4
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15

.text
.align 4

.global test_byteslice_roundtrip_asm
.type test_byteslice_roundtrip_asm, %function

// Test: input -> byteslice -> unbyteslice -> output
// Should return original data if both transformations are correct
test_byteslice_roundtrip_asm:
    // x0 = output
    // x1 = input
    
    // Save output pointer
    mov     x19, x0

    // Load 16 blocks from input
    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]
    ldp     q4, q5, [x1, #64]
    ldp     q6, q7, [x1, #96]
    ldp     q8, q9, [x1, #128]
    ldp     q10, q11, [x1, #160]
    ldp     q12, q13, [x1, #192]
    ldp     q14, q15, [x1, #224]

    // Forward byteslice
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Reverse byteslice (outunpack uses interleaved order)
    // C code line 800: byteslice_16x16b_fast(y0, y4, x0, x4, y1, y5, x1, x5, ...)
    // Where x0-x7 = v0-v7 (AB), y0-y7 = v8-v15 (CD)
    // So: v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7
    byteslice_16x16b v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7, v16, v17, v18, v19, v20, v21

    // Store 16 blocks to output
    stp     q8, q12, [x19, #0]
    stp     q0, q4, [x19, #32]
    stp     q9, q13, [x19, #64]
    stp     q1, q5, [x19, #96]
    stp     q10, q14, [x19, #128]
    stp     q2, q6, [x19, #160]
    stp     q11, q15, [x19, #192]
    stp     q3, q7, [x19, #224]

    ret
