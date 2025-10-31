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

.macro byteslice_16x16b_final a0, b0, c0, d0, a1, b1, c1, d1, \
                               a2, b2, c2, d2, a3, b3, c3, d3, \
                               t0, t1, t2, t3, t4, t5
    sub     sp, sp, #32
    st1     {\d2\().16b}, [sp]
    add     x11, sp, #16
    st1     {\d3\().16b}, [x11]
    
    transpose_4x4 \a0, \a1, \a2, \a3, \d2, \d3
    transpose_4x4 \b0, \b1, \b2, \b3, \d2, \d3
    
    ld1     {\d2\().16b}, [sp]
    ld1     {\d3\().16b}, [x11]
    ld1     {\a0\().16b}, [sp]
    ld1     {\a1\().16b}, [x11]
    
    transpose_4x4 \c0, \c1, \c2, \c3, \a0, \a1
    transpose_4x4 \d0, \d1, \d2, \d3, \a0, \a1
    
    adrp    x10, .Lshufb_16x16b
    add     x10, x10, :lo12:.Lshufb_16x16b
    ld1     {\a0\().16b}, [x10]
    ld1     {\a1\().16b}, [x11]
    
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
    
    st1     {\d3\().16b}, [x11]
    ld1     {\d3\().16b}, [sp]
    tbl     \a0\().16b, {\d3\().16b}, \a0\().16b
    ld1     {\d2\().16b}, [sp]
    
    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3
    
    st1     {\d2\().16b}, [sp]
    st1     {\d3\().16b}, [x11]
    ld1     {\b0\().16b}, [sp]
    ld1     {\b1\().16b}, [x11]
    
    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1
    
    st1     {\b0\().16b}, [sp]
    st1     {\b1\().16b}, [x11]
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
    byteslice_16x16b_final v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21
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
