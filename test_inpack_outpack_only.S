/* Test inpack + outunpack without any rounds */
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

    // Save d2, d3 BEFORE they're used as temporaries
    mov     \t4\().16b, \d2\().16b
    mov     \t5\().16b, \d3\().16b

    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3

    // Restore d2, d3 NOW
    mov     \d2\().16b, \t4\().16b
    mov     \d3\().16b, \t5\().16b

    // Save b0, b1 before they're used as temporaries
    mov     \t0\().16b, \b0\().16b
    mov     \t1\().16b, \b1\().16b

    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1

    // Restore b0, b1
    mov     \b0\().16b, \t0\().16b
    mov     \b1\().16b, \t1\().16b

    // NOTE: d2, d3 NOT restored - they have final values

.endm

.section .rodata
.align 4
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15

.text
.align 4

.global test_inpack_outpack_asm
.type test_inpack_outpack_asm, %function

// Test: input -> inpack -> outunpack -> output (should match input if both are correct)
// void test_inpack_outpack_asm(uint8_t *output, const uint8_t *input, uint64_t key)
test_inpack_outpack_asm:
    // x0 = output
    // x1 = input  
    // x2 = key

    // Save output pointer
    mov     x19, x0

    // Broadcast key
    dup     v31.2d, x2

    // Load 16 blocks and XOR with key (inpack pre-whitening)
    ldp     q16, q17, [x1, #0]
    ldp     q18, q19, [x1, #32]
    ldp     q20, q21, [x1, #64]
    ldp     q22, q23, [x1, #96]
    ldp     q24, q25, [x1, #128]
    ldp     q26, q27, [x1, #160]
    ldp     q28, q29, [x1, #192]
    ldp     q30, q0, [x1, #224]

    eor     v16.16b, v16.16b, v31.16b
    eor     v17.16b, v17.16b, v31.16b
    eor     v18.16b, v18.16b, v31.16b
    eor     v19.16b, v19.16b, v31.16b
    eor     v20.16b, v20.16b, v31.16b
    eor     v21.16b, v21.16b, v31.16b
    eor     v22.16b, v22.16b, v31.16b
    eor     v23.16b, v23.16b, v31.16b
    eor     v24.16b, v24.16b, v31.16b
    eor     v25.16b, v25.16b, v31.16b
    eor     v26.16b, v26.16b, v31.16b
    eor     v27.16b, v27.16b, v31.16b
    eor     v28.16b, v28.16b, v31.16b
    eor     v29.16b, v29.16b, v31.16b
    eor     v30.16b, v30.16b, v31.16b
    eor     v0.16b, v0.16b, v31.16b

    // Arrange in REVERSE order (like C code)
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
    mov     v15.16b, v0.16b
    mov     v0.16b, v16.16b

    // Forward byteslice
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Now v0-v7 = AB, v8-v15 = CD (byte-sliced)
    // For outunpack, need interleaved order: y0, y4, x0, x4, y1, y5, x1, x5, ...
    // Where x=AB (v0-v7), y=CD (v8-v15)
    // So: v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7

    // Reverse byteslice (outunpack)
    byteslice_16x16b v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7, v16, v17, v18, v19, v20, v21

    // Post-whitening: XOR in parameter order (not v0-v15!)
    // outunpack parameters: v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7
    eor     v8.16b, v8.16b, v31.16b
    eor     v12.16b, v12.16b, v31.16b
    eor     v0.16b, v0.16b, v31.16b
    eor     v4.16b, v4.16b, v31.16b
    eor     v9.16b, v9.16b, v31.16b
    eor     v13.16b, v13.16b, v31.16b
    eor     v1.16b, v1.16b, v31.16b
    eor     v5.16b, v5.16b, v31.16b
    eor     v10.16b, v10.16b, v31.16b
    eor     v14.16b, v14.16b, v31.16b
    eor     v2.16b, v2.16b, v31.16b
    eor     v6.16b, v6.16b, v31.16b
    eor     v11.16b, v11.16b, v31.16b
    eor     v15.16b, v15.16b, v31.16b
    eor     v3.16b, v3.16b, v31.16b
    eor     v7.16b, v7.16b, v31.16b

    // Write output: C writes x7,x6,x5,x4,x3,x2,x1,x0,x15,x14,x13,x12,x11,x10,x9,x8
    // In C, after outunpack these variables contain unbytesliced blocks
    // The mapping depends on the complete transformation chain
    // For now, use the same order as main code
    stp     q7, q6, [x19, #0]
    stp     q5, q4, [x19, #32]
    stp     q3, q2, [x19, #64]
    stp     q1, q0, [x19, #96]
    stp     q15, q14, [x19, #128]
    stp     q13, q12, [x19, #160]
    stp     q11, q10, [x19, #192]
    stp     q9, q8, [x19, #224]

    ret
