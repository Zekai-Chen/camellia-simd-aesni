.text
.global test_simple_whitening_asm
.type test_simple_whitening_asm, %function
.align 4
test_simple_whitening_asm:
    // Load one block
    ldr     q0, [x1]

    // XOR with key
    dup     v1.2d, x2
    eor     v0.16b, v0.16b, v1.16b

    // Store result
    str     q0, [x0]

    ret
.size test_simple_whitening_asm, .-test_simple_whitening_asm
