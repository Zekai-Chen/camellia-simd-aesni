/*
 * Assembly wrapper for isolated byteslice test
 */
.text
.align 4

.globl test_byteslice_16x16b_asm
.type test_byteslice_16x16b_asm, %function

/*
 * test_byteslice_16x16b_asm - Assembly byteslice wrapper
 *
 * Parameters:
 *   x0 = output buffer (256 bytes)
 *   x1 = input buffer (256 bytes)
 */
test_byteslice_16x16b_asm:
    // Save output pointer
    mov     x10, x0

    // Load 16 input blocks into v0-v15
    ldp     q0, q1, [x1, #0]        // blocks 0-1
    ldp     q2, q3, [x1, #32]       // blocks 2-3
    ldp     q4, q5, [x1, #64]       // blocks 4-5
    ldp     q6, q7, [x1, #96]       // blocks 6-7
    ldp     q8, q9, [x1, #128]      // blocks 8-9
    ldp     q10, q11, [x1, #160]    // blocks 10-11
    ldp     q12, q13, [x1, #192]    // blocks 12-13
    ldp     q14, q15, [x1, #224]    // blocks 14-15

    // Call byteslice_16x16b macro
    // Parameters: a0-d0, a1-d1, a2-d2, a3-d3, t0-t5
    // a0=v0, b0=v1, c0=v2, d0=v3, a1=v4, b1=v5, c1=v6, d1=v7,
    // a2=v8, b2=v9, c2=v10, d2=v11, a3=v12, b3=v13, c3=v14, d3=v15
    // t0-t5 = v16-v21
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Store 16 output blocks
    stp     q0, q1, [x10, #0]       // a0, b0
    stp     q2, q3, [x10, #32]      // c0, d0
    stp     q4, q5, [x10, #64]      // a1, b1
    stp     q6, q7, [x10, #96]      // c1, d1
    stp     q8, q9, [x10, #128]     // a2, b2
    stp     q10, q11, [x10, #160]   // c2, d2
    stp     q12, q13, [x10, #192]   // a3, b3
    stp     q14, q15, [x10, #224]   // c3, d3

    ret
.size test_byteslice_16x16b_asm, .-test_byteslice_16x16b_asm
