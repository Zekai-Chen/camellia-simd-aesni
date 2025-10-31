.text
.include "camellia_simd128_aarch64_neon_crypto.S"

.align 4
.globl test_debug_block_order
.type test_debug_block_order,%function

test_debug_block_order:
    // x0 = output pointer
    // x1 = input pointer
    
    sub     sp, sp, #512
    stp     x29, x30, [sp]
    mov     x29, sp
    
    // Load 16 input blocks
    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]
    ldp     q4, q5, [x1, #64]
    ldp     q6, q7, [x1, #96]
    ldp     q8, q9, [x1, #128]
    ldp     q10, q11, [x1, #160]
    ldp     q12, q13, [x1, #192]
    ldp     q14, q15, [x1, #224]
    
    mov     x20, x0
    
    // Forward byteslice (sequential params)
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21
    
    // Reverse byteslice (interleaved params)
    byteslice_16x16b v8, v12, v0, v4, v9, v13, v1, v5, v10, v14, v2, v6, v11, v15, v3, v7, v16, v17, v18, v19, v20, v21
    
    // Store in SEQUENTIAL order (NO reversal) to debug
    str     q0, [x20, #0*16]
    str     q1, [x20, #1*16]
    str     q2, [x20, #2*16]
    str     q3, [x20, #3*16]
    str     q4, [x20, #4*16]
    str     q5, [x20, #5*16]
    str     q6, [x20, #6*16]
    str     q7, [x20, #7*16]
    str     q8, [x20, #8*16]
    str     q9, [x20, #9*16]
    str     q10, [x20, #10*16]
    str     q11, [x20, #11*16]
    str     q12, [x20, #12*16]
    str     q13, [x20, #13*16]
    str     q14, [x20, #14*16]
    str     q15, [x20, #15*16]
    
    ldp     x29, x30, [sp]
    add     sp, sp, #512
    ret
