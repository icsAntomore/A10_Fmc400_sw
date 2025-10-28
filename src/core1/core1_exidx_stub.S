/*
 * Provide a minimal function with unwind metadata so that the
 * linker always emits a valid .ARM.exidx table for the core1 image.
 *
 * Without at least one entry, the final ELF ends up with an empty
 * .ARM.exidx section whose sh_link field is left unset.  Recent
 * binutils warn about that situation and the build is reported as
 * failed.  The stub below forces the assembler to emit a cantunwind
 * record that references the stub function, which keeps the linker
 * happy without adding any meaningful code to the firmware.
 */

    .syntax unified
    .arm

    .section .text.__core1_unwind_stub, "ax", %progbits
    .global __core1_unwind_stub
    .type   __core1_unwind_stub, %function
    .hidden __core1_unwind_stub
__core1_unwind_stub:
    .fnstart
    bx      lr
    .cantunwind
    .fnend

    .size   __core1_unwind_stub, .-__core1_unwind_stub

 /*
     * Binutils 2.34+ warns when the .ARM.exidx section does not have
     * a valid sh_link pointing at a companion .ARM.extab section.
     * The directives above generate the exidx records, but nothing in
     * this bare-metal image emits an extab.  Provide an explicit (and
     * intentionally empty) one here so that the linker always sets the
     * sh_link field correctly and the build stays warning-free.
     */
    .section .ARM.extab, "a", %progbits
    .align  2
    .global __core1_extab_start
    .global __core1_extab_end
__core1_extab_start:
__core1_extab_end:
