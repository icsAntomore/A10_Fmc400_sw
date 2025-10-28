/* core1_vectors.S — ARM state (NO Thumb) */

    .syntax unified
    .arm                      /* <<< ARM state */
    .arch armv7-a

    .section .vectors, "ax"
    .align 5
    .global __core1_vectors
    .extern _start_core1
    .extern core1_irq_handler_c   /* C handler compilato in ARM (default) */

__core1_vectors:
    b   _start_core1         /* Reset */
    b   __vect_undef         /* Undefined */
    b   __vect_swi           /* SVC/SWI */
    b   __vect_pabt          /* Prefetch abort */
    b   __vect_dabt          /* Data abort */
    b   __vect_reserved      /* Reserved */
    b   core1_irq_entry      /* IRQ */
    b   __vect_fiq           /* FIQ */
    .size __core1_vectors, .-__core1_vectors

    .text
    .align 2

__vect_undef:    b __vect_undef
__vect_swi:      b __vect_swi
__vect_pabt:     b __vect_pabt
__vect_dabt:     b __vect_dabt
__vect_reserved: b __vect_reserved
__vect_fiq:      b __vect_fiq

/* IRQ entry minimale in ARM state */
    .global core1_irq_entry
core1_irq_entry:
    /* salva un minimo di contesto */
    sub     sp, sp, #24
    stm     sp, {r0-r3, r12, lr}

    bl      core1_irq_handler_c   /* legge IAR, serve PPI29, EOI */

    /* ripristina e ritorna dall'IRQ */
    ldm     sp, {r0-r3, r12, lr}
    add     sp, sp, #24
    subs    pc, lr, #4            /* restore CPSR da SPSR_irq e rientra */
