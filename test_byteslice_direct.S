/* Direct byteslice test - standalone assembly test */

// Include the macros we need first
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

    // Phase 1: First level of transpose (4x4 groups)
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

// Shuffle pattern
.section .rodata
.align 4
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15

.text
.align 4

.global test_byteslice_asm
.type test_byteslice_asm, %function

// void test_byteslice_asm(uint8_t *output, const uint8_t *input)
// x0 = output (256 bytes, 16 vectors)
// x1 = input (256 bytes, 16 blocks)
test_byteslice_asm:
    // Load 16 blocks from input IN REVERSE ORDER (like C code)
    // C loads: y7=blk0, y6=blk1, ..., y0=blk7, x7=blk8, ..., x0=blk15
    // Then calls byteslice(x0..x7, y0..y7) = byteslice(blk15..blk8, blk7..blk0)
    ldp     q16, q17, [x1, #0]          // Blocks 0-1
    ldp     q18, q19, [x1, #32]         // Blocks 2-3
    ldp     q20, q21, [x1, #64]         // Blocks 4-5
    ldp     q22, q23, [x1, #96]         // Blocks 6-7
    ldp     q24, q25, [x1, #128]        // Blocks 8-9
    ldp     q26, q27, [x1, #160]        // Blocks 10-11
    ldp     q28, q29, [x1, #192]        // Blocks 12-13
    ldp     q30, q31, [x1, #224]        // Blocks 14-15

    // Arrange for byteslice: v0..v7 = blocks 15..8, v8..v15 = blocks 7..0
    mov     v0.16b, v31.16b             // Block 15
    mov     v1.16b, v30.16b             // Block 14
    mov     v2.16b, v29.16b             // Block 13
    mov     v3.16b, v28.16b             // Block 12
    mov     v4.16b, v27.16b             // Block 11
    mov     v5.16b, v26.16b             // Block 10
    mov     v6.16b, v25.16b             // Block 9
    mov     v7.16b, v24.16b             // Block 8
    mov     v8.16b, v23.16b             // Block 7
    mov     v9.16b, v22.16b             // Block 6
    mov     v10.16b, v21.16b            // Block 5
    mov     v11.16b, v20.16b            // Block 4
    mov     v12.16b, v19.16b            // Block 3
    mov     v13.16b, v18.16b            // Block 2
    mov     v14.16b, v17.16b            // Block 1
    mov     v15.16b, v16.16b            // Block 0

    // Call byteslice transformation
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Store 16 vectors to output
    stp     q0, q1, [x0, #0]
    stp     q2, q3, [x0, #32]
    stp     q4, q5, [x0, #64]
    stp     q6, q7, [x0, #96]
    stp     q8, q9, [x0, #128]
    stp     q10, q11, [x0, #160]
    stp     q12, q13, [x0, #192]
    stp     q14, q15, [x0, #224]

    ret
