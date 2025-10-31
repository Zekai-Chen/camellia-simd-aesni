/* Test inpack dump */
.text

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

.global test_inpack_dump_asm
.type test_inpack_dump_asm, %function

// void test_inpack_dump_asm(uint8_t *ab_out, uint8_t *cd_out, const uint8_t *input, uint64_t key)
test_inpack_dump_asm:
    // x0 = ab_out
    // x1 = cd_out  
    // x2 = input
    // x3 = key

    // Save output pointers
    mov     x19, x0
    mov     x20, x1

    // Broadcast pre-whitening key
    dup     v31.2d, x3

    // Load 16 blocks and XOR with key
    ldp     q16, q17, [x2, #0]
    ldp     q18, q19, [x2, #32]
    ldp     q20, q21, [x2, #64]
    ldp     q22, q23, [x2, #96]

    eor     v16.16b, v16.16b, v31.16b
    eor     v17.16b, v17.16b, v31.16b
    eor     v18.16b, v18.16b, v31.16b
    eor     v19.16b, v19.16b, v31.16b
    eor     v20.16b, v20.16b, v31.16b
    eor     v21.16b, v21.16b, v31.16b
    eor     v22.16b, v22.16b, v31.16b
    eor     v23.16b, v23.16b, v31.16b

    ldp     q24, q25, [x2, #128]
    ldp     q26, q27, [x2, #160]
    ldp     q28, q29, [x2, #192]
    ldp     q30, q0, [x2, #224]

    eor     v24.16b, v24.16b, v31.16b
    eor     v25.16b, v25.16b, v31.16b
    eor     v26.16b, v26.16b, v31.16b
    eor     v27.16b, v27.16b, v31.16b
    eor     v28.16b, v28.16b, v31.16b
    eor     v29.16b, v29.16b, v31.16b
    eor     v30.16b, v30.16b, v31.16b
    eor     v0.16b, v0.16b, v31.16b

    // Arrange: v0-v7 = blocks 0-7, v8-v15 = blocks 8-15
    mov     v1.16b, v17.16b
    mov     v2.16b, v18.16b
    mov     v3.16b, v19.16b
    mov     v4.16b, v20.16b
    mov     v5.16b, v21.16b
    mov     v6.16b, v22.16b
    mov     v7.16b, v23.16b
    mov     v0.16b, v16.16b

    mov     v8.16b, v24.16b
    mov     v9.16b, v25.16b
    mov     v10.16b, v26.16b
    mov     v11.16b, v27.16b
    mov     v12.16b, v28.16b
    mov     v13.16b, v29.16b
    mov     v14.16b, v30.16b
    // v15 from v0 (block 15), need to save first
    mov     v15.16b, v0.16b
    mov     v0.16b, v16.16b

    // Byteslice
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Store AB (v0-v7) and CD (v8-v15)
    stp     q0, q1, [x19, #0]
    stp     q2, q3, [x19, #32]
    stp     q4, q5, [x19, #64]
    stp     q6, q7, [x19, #96]

    stp     q8, q9, [x20, #0]
    stp     q10, q11, [x20, #32]
    stp     q12, q13, [x20, #64]
    stp     q14, q15, [x20, #96]

    ret
