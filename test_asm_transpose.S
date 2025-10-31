.text
.align 4

// Import transpose_4x4 macro
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

.globl test_asm_transpose_4x4
.type test_asm_transpose_4x4, %function
test_asm_transpose_4x4:
    // x0 = pointer to 64 bytes (4 blocks)
    ldp     q0, q1, [x0, #0]
    ldp     q2, q3, [x0, #32]

    // Call transpose macro
    transpose_4x4 v0, v1, v2, v3, v4, v5

    // Store result
    stp     q0, q1, [x0, #0]
    stp     q2, q3, [x0, #32]

    ret
.size test_asm_transpose_4x4, .-test_asm_transpose_4x4
