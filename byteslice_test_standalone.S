/*
 * Standalone byteslice test - contains only byteslice macro and dependencies
 */
.data
.align 4

/* Shuffle pattern for byte-slicing (16x16b transpose) */
.Lshufb_16x16b:
    .byte 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15

.text
.align 4

/*
 * transpose_4x4 - Transpose 4x4 matrix of 32-bit elements
 */
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

/*
 * byteslice_16x16b - Convert 16 blocks from normal to byte-sliced format
 */
.macro byteslice_16x16b a0, b0, c0, d0, a1, b1, c1, d1, \
                        a2, b2, c2, d2, a3, b3, c3, d3, \
                        t0, t1, t2, t3, t4, t5

    /*
     * Phase 1: First level of transpose (4x4 groups)
     * C code lines 709-719
     */

    // Transpose Group 0 (a0-a3) and Group 1 (b0-b3) using d2, d3 as temps
    // This corrupts d2, d3 with temporary values
    transpose_4x4 \a0, \a1, \a2, \a3, \d2, \d3
    transpose_4x4 \b0, \b1, \b2, \b3, \d2, \d3

    // Save the temp results from d2, d3 (C line 713-714: st0=d2, st1=d3)
    mov     \t4\().16b, \d2\().16b
    mov     \t5\().16b, \d3\().16b

    // Load temp results into a0, a1 (C line 716-717: a0=st0, a1=st1)
    // This DISCARDS the transposed a0, a1 values!
    mov     \a0\().16b, \t4\().16b
    mov     \a1\().16b, \t5\().16b

    // Transpose Group 2 (c0-c3) and Group 3 (d0-d3) using a0, a1 as temps
    // a0, a1 now contain the d2, d3 temp results from Group 0-1 transposes
    transpose_4x4 \c0, \c1, \c2, \c3, \a0, \a1
    transpose_4x4 \d0, \d1, \d2, \d3, \a0, \a1

    /*
     * Phase 2: Shuffle bytes within vectors
     * C code lines 721-741
     */

    // Load shuffle pattern (C line 721: shufb_16x16b_stack = a0)
    // Wait, that's backwards! Let me re-read...
    // C line 721: vmovdqa128(shufb_16x16b_stack, a0) means a0 = shufb_16x16b_stack
    // So a0 gets the shuffle pattern
    adrp    x10, .Lshufb_16x16b
    add     x10, x10, :lo12:.Lshufb_16x16b
    ld1     {\a0\().16b}, [x10]

    // Save a1 to t3 (C line 722: st1 = a1)
    // a1 currently contains d3 temp result from Phase 1
    mov     \t3\().16b, \a1\().16b

    // Shuffle 13 vectors using a0 as pattern (C lines 723-735)
    tbl     \a2\().16b, {\a2\().16b}, \a0\().16b
    tbl     \a3\().16b, {\a3\().16b}, \a0\().16b
    tbl     \b0\().16b, {\b0\().16b}, \a0\().16b
    tbl     \b1\().16b, {\b1\().16b}, \a0\().16b
    tbl     \b2\().16b, {\b2\().16b}, \a0\().16b
    tbl     \b3\().16b, {\b3\().16b}, \a0\().16b
    tbl     \a1\().16b, {\t3\().16b}, \a0\().16b    // shuffle saved a1
    tbl     \c0\().16b, {\c0\().16b}, \a0\().16b
    tbl     \c1\().16b, {\c1\().16b}, \a0\().16b
    tbl     \c2\().16b, {\c2\().16b}, \a0\().16b
    tbl     \c3\().16b, {\c3\().16b}, \a0\().16b
    tbl     \d0\().16b, {\d0\().16b}, \a0\().16b
    tbl     \d1\().16b, {\d1\().16b}, \a0\().16b

    // Handle d2, d3 (C lines 736-737)
    tbl     \d2\().16b, {\d2\().16b}, \a0\().16b
    tbl     \d3\().16b, {\d3\().16b}, \a0\().16b

    // Save d3 to t3, then shuffle the old a0 value (C lines 738-740)
    // The old a0 is in t4 from Phase 1
    mov     \t3\().16b, \d3\().16b          // C line 738: d3 = st1 (but st1 has shuffled a1, not d3)
    // Wait, this is confusing. Let me re-read the C code carefully.

    // Actually C line 738: vmovdqa128(d3, st1) means st1 = d3 (save shuffled d3)
    // C line 739: vmovdqa128(st0, d3) means d3 = st0 (load... what?)
    // At this point st0 contains the d2 temp from Phase 1 line 713
    // So d3 = d2 temp from Phase 1? That doesn't make sense.

    // Let me think... maybe st0 was updated somewhere? No, Phase 2 doesn't write to st0.
    // So st0 still contains d2 temp from line 713.
    // C line 739 loads that into d3.
    // C line 740 shuffles d3 (which now contains d2 temp) into a0.
    // C line 741 loads st0 (d2 temp) into d2.

    // This is very confusing. Let me just follow the C code exactly:
    mov     \t5\().16b, \d3\().16b          // st1 = d3 (save shuffled d3)
    mov     \d3\().16b, \t4\().16b          // d3 = st0 (d2 temp from Phase 1)
    tbl     \a0\().16b, {\d3\().16b}, \a0\().16b    // shuffle d3 into a0
    mov     \d2\().16b, \t4\().16b          // d2 = st0 (d2 temp from Phase 1)

    // Now save shuffled d2, d3 for Phase 3
    // d2 was just loaded (not shuffled), d3 is in t5 (shuffled)
    // So t4 should hold shuffled d2? But we just loaded d2 from t4...
    //
    // I think I'm misunderstanding. Let me save both now:
    mov     \t4\().16b, \d2\().16b          // Save d2 (loaded from old st0)
    // t5 already has shuffled d3

    /*
     * Phase 3: Second level of transpose
     */

    // Transpose Group 0 and 1 (will corrupt d2, d3)
    transpose_4x4 \a0, \b0, \c0, \d0, \d2, \d3
    transpose_4x4 \a1, \b1, \c1, \d1, \d2, \d3

    // Restore shuffled d2, d3 NOW (before Group 2-3 transposes need them!)
    mov     \d2\().16b, \t4\().16b
    mov     \d3\().16b, \t5\().16b

    // Save b0, b1 and transpose Group 2-3
    mov     \t0\().16b, \b0\().16b
    mov     \t1\().16b, \b1\().16b
    transpose_4x4 \a2, \b2, \c2, \d2, \b0, \b1
    transpose_4x4 \a3, \b3, \c3, \d3, \b0, \b1

    // Restore b0, b1
    mov     \b0\().16b, \t0\().16b
    mov     \b1\().16b, \t1\().16b

.endm

/*
 * test_byteslice_16x16b_asm - Test wrapper
 */
.globl test_byteslice_16x16b_asm
.type test_byteslice_16x16b_asm, %function
test_byteslice_16x16b_asm:
    mov     x10, x0

    // Load input
    ldp     q0, q1, [x1, #0]
    ldp     q2, q3, [x1, #32]
    ldp     q4, q5, [x1, #64]
    ldp     q6, q7, [x1, #96]
    ldp     q8, q9, [x1, #128]
    ldp     q10, q11, [x1, #160]
    ldp     q12, q13, [x1, #192]
    ldp     q14, q15, [x1, #224]

    // Call byteslice
    byteslice_16x16b v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21

    // Store output
    stp     q0, q1, [x10, #0]
    stp     q2, q3, [x10, #32]
    stp     q4, q5, [x10, #64]
    stp     q6, q7, [x10, #96]
    stp     q8, q9, [x10, #128]
    stp     q10, q11, [x10, #160]
    stp     q12, q13, [x10, #192]
    stp     q14, q15, [x10, #224]

    ret
.size test_byteslice_16x16b_asm, .-test_byteslice_16x16b_asm
