/*
 * Global debug buffers to capture register values
 */

#ifdef __aarch64__

.section .bss
.align 4

.global debug_v0
.global debug_v1
.global debug_v2
.global debug_v3
.global debug_v4
.global debug_v5
.global debug_v6
.global debug_v7
.global debug_v8
.global debug_v9
.global debug_v10
.global debug_v11
.global debug_v12
.global debug_v13
.global debug_v14
.global debug_v15

debug_v0:  .space 16
debug_v1:  .space 16
debug_v2:  .space 16
debug_v3:  .space 16
debug_v4:  .space 16
debug_v5:  .space 16
debug_v6:  .space 16
debug_v7:  .space 16
debug_v8:  .space 16
debug_v9:  .space 16
debug_v10: .space 16
debug_v11: .space 16
debug_v12: .space 16
debug_v13: .space 16
debug_v14: .space 16
debug_v15: .space 16

.global debug_cd_after_first_round
debug_cd_after_first_round: .space 128

.global debug_ab_before_first_round
debug_ab_before_first_round: .space 128

.global debug_cd_after_byteslice
debug_cd_after_byteslice: .space 128

.global debug_phase1_c0
.global debug_phase1_d2
.global debug_phase2_c0
.global debug_phase2_d2
.global debug_phase3_c0
.global debug_phase3_d2

debug_phase1_c0: .space 16
debug_phase1_d2: .space 16
debug_phase2_c0: .space 16
debug_phase2_d2: .space 16
debug_phase3_c0: .space 16
debug_phase3_d2: .space 16

.global debug_byteslice_input_v8
debug_byteslice_input_v8: .space 16

#endif

// Additional debug variables for FL/FLINV
.globl debug_ab_before_fl
.globl debug_ab_after_fl
.globl debug_cd_before_fl
.globl debug_cd_after_fl

.bss
.align 4
debug_ab_before_fl:
    .space 128
debug_ab_after_fl:
    .space 128
debug_cd_before_fl:
    .space 128
debug_cd_after_fl:
    .space 128

.globl debug_cd_before_byteslice
.bss
.align 4
debug_cd_before_byteslice:
    .space 128

// Debug variables for roundsm16 phases
.globl debug_ab_on_stack_after_store
.globl debug_stack_memory_before_load
.globl debug_ab_loaded_from_stack
.globl debug_roundsm16_phase0_ab
.globl debug_roundsm16_phase1_ab
.globl debug_roundsm16_phase2_ab
.globl debug_roundsm16_phase3_ab
.globl debug_roundsm16_phase4_ab

.bss
.align 4
debug_ab_on_stack_after_store:
    .space 128
debug_stack_memory_before_load:
    .space 128
debug_ab_loaded_from_stack:
    .space 128
debug_roundsm16_phase0_ab:
    .space 128
debug_roundsm16_phase1_ab:
    .space 128
debug_roundsm16_phase2_ab:
    .space 128
debug_roundsm16_phase3_ab:
    .space 128
debug_roundsm16_phase4_ab:
    .space 128
