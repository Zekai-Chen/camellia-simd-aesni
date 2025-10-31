/*
 * Debug function to capture AB and CD register values
 * before the byteslice operation
 */

#ifdef __aarch64__

.arch armv8-a+crypto
.text
.align 4

/*
 * camellia_encrypt_16blks_debug_ab_cd - Debug wrapper
 * Parameters:
 *   x0: ctx pointer
 *   x1: output pointer
 *   x2: input pointer
 *   x3: ab_v0_out (16 bytes) - save v0 here
 *   x4: ab_v7_out (16 bytes) - save v7 here
 *   x5: cd_v8_out (16 bytes) - save v8 here
 *   x6: cd_v15_out (16 bytes) - save v15 here
 */
.global camellia_encrypt_16blks_debug_ab_cd
.type camellia_encrypt_16blks_debug_ab_cd, %function
.align 4
camellia_encrypt_16blks_debug_ab_cd:
    // Save callee-saved registers and output buffer pointers
    stp     x29, x30, [sp, #-64]!
    stp     x19, x20, [sp, #16]
    stp     x21, x22, [sp, #32]
    stp     x23, x24, [sp, #48]

    // Save output buffer pointers to callee-saved registers
    mov     x19, x3                     // ab_v0_out
    mov     x20, x4                     // ab_v7_out
    mov     x21, x5                     // cd_v8_out
    mov     x22, x6                     // cd_v15_out

    // Call the main encryption function (will modify v0-v15)
    bl      camellia_encrypt_16blks_simd128_aarch64_asm

    // At this point, the encryption is done and output is written
    // We can't capture the intermediate values this way...
    // We need a different approach

    // Restore registers and return
    ldp     x23, x24, [sp, #48]
    ldp     x21, x22, [sp, #32]
    ldp     x19, x20, [sp, #16]
    ldp     x29, x30, [sp], #64
    ret

.size camellia_encrypt_16blks_debug_ab_cd, .-camellia_encrypt_16blks_debug_ab_cd

#endif
