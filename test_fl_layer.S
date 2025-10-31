/*
 * Test wrapper for FL layer macro
 */

.text
.align 4

.global test_fl_layer_asm
.type test_fl_layer_asm, %function

/*
 * test_fl_layer_asm(uint64_t *ab_state, uint64_t *cd_state,
 *                    uint64_t *key_fl, uint64_t *key_flinv)
 * Parameters:
 *   x0: pointer to AB state (8 vectors = 128 bytes)
 *   x1: pointer to CD state (8 vectors = 128 bytes)
 *   x2: pointer to FL key (2 uint64_t = 16 bytes)
 *   x3: pointer to FLINV key (2 uint64_t = 16 bytes)
 */
test_fl_layer_asm:
    // Save callee-saved registers
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp

    // Load AB state from memory into v0-v7
    ldp     q0, q1, [x0, #0]
    ldp     q2, q3, [x0, #32]
    ldp     q4, q5, [x0, #64]
    ldp     q6, q7, [x0, #96]

    // x1 = cd_state pointer (already in x1)
    // x2 = key_fl pointer
    // x3 = key_flinv pointer

    // Call fls16 macro
    // fls16 l0, l1, l2, l3, l4, l5, l6, l7, t0, t1, t2, t3, tt0, tt1, tt2, tt3, kl, kr, cd_ptr
    // Parameters: v0-v7 (AB), v16-v23 (temps), x2 (kl), x3 (kr), x1 (cd_ptr)
    fls16 v0, v1, v2, v3, v4, v5, v6, v7, v16, v17, v18, v19, v20, v21, v22, v23, x2, x3, x1

    // Store AB state back to memory
    stp     q0, q1, [x0, #0]
    stp     q2, q3, [x0, #32]
    stp     q4, q5, [x0, #64]
    stp     q6, q7, [x0, #96]

    // Restore and return
    ldp     x29, x30, [sp], #16
    ret

.size test_fl_layer_asm, .-test_fl_layer_asm

// Note: This file should be compiled together with camellia_simd128_aarch64_neon_crypto.S
// to have access to the fls16 macro definition
