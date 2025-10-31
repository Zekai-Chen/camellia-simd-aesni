/* Test load + XOR + XOR + write (should restore original) */
.text

.global test_double_xor_asm
.type test_double_xor_asm, %function

// void test_double_xor_asm(uint8_t *output, const uint8_t *input, uint64_t key)
test_double_xor_asm:
    // x0 = output
    // x1 = input
    // x2 = key

    // Broadcast key
    dup     v31.2d, x2

    // Load 16 blocks
    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]
    ldp     q4, q5, [x1, #64]
    ldp     q6, q7, [x1, #96]
    ldp     q8, q9, [x1, #128]
    ldp     q10, q11, [x1, #160]
    ldp     q12, q13, [x1, #192]
    ldp     q14, q15, [x1, #224]

    // XOR with key (first time)
    eor     v0.16b, v0.16b, v31.16b
    eor     v1.16b, v1.16b, v31.16b
    eor     v2.16b, v2.16b, v31.16b
    eor     v3.16b, v3.16b, v31.16b
    eor     v4.16b, v4.16b, v31.16b
    eor     v5.16b, v5.16b, v31.16b
    eor     v6.16b, v6.16b, v31.16b
    eor     v7.16b, v7.16b, v31.16b
    eor     v8.16b, v8.16b, v31.16b
    eor     v9.16b, v9.16b, v31.16b
    eor     v10.16b, v10.16b, v31.16b
    eor     v11.16b, v11.16b, v31.16b
    eor     v12.16b, v12.16b, v31.16b
    eor     v13.16b, v13.16b, v31.16b
    eor     v14.16b, v14.16b, v31.16b
    eor     v15.16b, v15.16b, v31.16b

    // XOR with key (second time - should restore)
    eor     v0.16b, v0.16b, v31.16b
    eor     v1.16b, v1.16b, v31.16b
    eor     v2.16b, v2.16b, v31.16b
    eor     v3.16b, v3.16b, v31.16b
    eor     v4.16b, v4.16b, v31.16b
    eor     v5.16b, v5.16b, v31.16b
    eor     v6.16b, v6.16b, v31.16b
    eor     v7.16b, v7.16b, v31.16b
    eor     v8.16b, v8.16b, v31.16b
    eor     v9.16b, v9.16b, v31.16b
    eor     v10.16b, v10.16b, v31.16b
    eor     v11.16b, v11.16b, v31.16b
    eor     v12.16b, v12.16b, v31.16b
    eor     v13.16b, v13.16b, v31.16b
    eor     v14.16b, v14.16b, v31.16b
    eor     v15.16b, v15.16b, v31.16b

    // Write 16 blocks back
    stp     q0, q1, [x0, #0]
    stp     q2, q3, [x0, #32]
    stp     q4, q5, [x0, #64]
    stp     q6, q7, [x0, #96]
    stp     q8, q9, [x0, #128]
    stp     q10, q11, [x0, #160]
    stp     q12, q13, [x0, #192]
    stp     q14, q15, [x0, #224]

    ret
