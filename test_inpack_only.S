/*
 * Test inpack module only - standalone assembly test
 */
.text
.arch armv8-a+crypto

.globl test_inpack_asm
.type test_inpack_asm, %function

/*
 * Test function: only do inpack
 * void test_inpack_asm(uint64_t prewhite_key, uint8_t *output_ab, uint8_t *output_cd, const uint8_t *input)
 * x0 = prewhite_key
 * x1 = output_ab (128 bytes for AB state)
 * x2 = output_cd (128 bytes for CD state)
 * x3 = input (256 bytes)
 */
test_inpack_asm:
    // Save registers
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // Broadcast pre-whitening key
    dup     v31.2d, x0

    // Load 16 input blocks and XOR with pre-whitening key
    ldp     q16, q17, [x3, #0]
    ldp     q18, q19, [x3, #32]
    ldp     q20, q21, [x3, #64]
    ldp     q22, q23, [x3, #96]
    ldp     q24, q25, [x3, #128]
    ldp     q26, q27, [x3, #160]
    ldp     q28, q29, [x3, #192]
    ldp     q30, q0, [x3, #224]

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

    // Forward byteslice transformation
    // byteslice_16x16b(v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v0, v1, v2)

    // 4x4 transpose for first group (v16-v19)
    trn1    v1.4s, v16.4s, v17.4s
    trn2    v2.4s, v16.4s, v17.4s
    trn1    v3.4s, v18.4s, v19.4s
    trn2    v4.4s, v18.4s, v19.4s
    trn1    v16.2d, v1.2d, v3.2d
    trn2    v17.2d, v1.2d, v3.2d
    trn1    v18.2d, v2.2d, v4.2d
    trn2    v19.2d, v2.2d, v4.2d

    // 4x4 transpose for second group (v20-v23)
    trn1    v1.4s, v20.4s, v21.4s
    trn2    v2.4s, v20.4s, v21.4s
    trn1    v3.4s, v22.4s, v23.4s
    trn2    v4.4s, v22.4s, v23.4s
    trn1    v20.2d, v1.2d, v3.2d
    trn2    v21.2d, v1.2d, v3.2d
    trn1    v22.2d, v2.2d, v4.2d
    trn2    v23.2d, v2.2d, v4.2d

    // 4x4 transpose for third group (v24-v27)
    trn1    v1.4s, v24.4s, v25.4s
    trn2    v2.4s, v24.4s, v25.4s
    trn1    v3.4s, v26.4s, v27.4s
    trn2    v4.4s, v26.4s, v27.4s
    trn1    v24.2d, v1.2d, v3.2d
    trn2    v25.2d, v1.2d, v3.2d
    trn1    v26.2d, v2.2d, v4.2d
    trn2    v27.2d, v2.2d, v4.2d

    // 4x4 transpose for fourth group (v28-v30, v0)
    trn1    v1.4s, v28.4s, v29.4s
    trn2    v2.4s, v28.4s, v29.4s
    trn1    v3.4s, v30.4s, v0.4s
    trn2    v4.4s, v30.4s, v0.4s
    trn1    v28.2d, v1.2d, v3.2d
    trn2    v29.2d, v1.2d, v3.2d
    trn1    v30.2d, v2.2d, v4.2d
    trn2    v0.2d, v2.2d, v4.2d

    // 16x16 matrix transformation
    trn1    v1.2d, v16.2d, v24.2d
    trn2    v2.2d, v16.2d, v24.2d
    trn1    v3.2d, v17.2d, v25.2d
    trn2    v4.2d, v17.2d, v25.2d
    trn1    v5.2d, v18.2d, v26.2d
    trn2    v6.2d, v18.2d, v26.2d
    trn1    v7.2d, v19.2d, v27.2d
    trn2    v8.2d, v19.2d, v27.2d
    trn1    v9.2d, v20.2d, v28.2d
    trn2    v10.2d, v20.2d, v28.2d
    trn1    v11.2d, v21.2d, v29.2d
    trn2    v12.2d, v21.2d, v29.2d
    trn1    v13.2d, v22.2d, v30.2d
    trn2    v14.2d, v22.2d, v30.2d
    trn1    v15.2d, v23.2d, v0.2d
    trn2    v16.2d, v23.2d, v0.2d

    // Move results: v1-v8 = AB, v9-v16 = CD
    mov     v0.16b, v1.16b
    mov     v1.16b, v3.16b
    mov     v2.16b, v5.16b
    mov     v3.16b, v7.16b
    mov     v4.16b, v9.16b
    mov     v5.16b, v11.16b
    mov     v6.16b, v13.16b
    mov     v7.16b, v15.16b

    // Store AB (v0-v7)
    stp     q0, q1, [x1, #0]
    stp     q2, q3, [x1, #32]
    stp     q4, q5, [x1, #64]
    stp     q6, q7, [x1, #96]

    // Store CD (v2,v4,v6,v8,v10,v12,v14,v16)
    stp     q2, q4, [x2, #0]
    stp     q6, q8, [x2, #32]
    stp     q10, q12, [x2, #64]
    stp     q14, q16, [x2, #96]

    // Restore and return
    ldp     x29, x30, [sp], #16
    ret
.size test_inpack_asm, .-test_inpack_asm
