/*
 * Assembly transpose test wrapper
 */
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

.globl transpose_4x4_asm
.type transpose_4x4_asm, %function
transpose_4x4_asm:
    // x0 = output, x1 = input
    mov     x10, x0

    // Load input
    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]

    // Transpose
    transpose_4x4 v0, v1, v2, v3, v4, v5

    // Store output
    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]

    ret
.size transpose_4x4_asm, .-transpose_4x4_asm
