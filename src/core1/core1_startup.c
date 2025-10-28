#include <stdint.h>
#include <stdbool.h>
#include "core_boot_mailbox.h"   // core1_startaddr_mailbox(), A10_ROM_CPU1START_ADDR
#include "shared_ipc.h"          // SHM_CTRL, SHM_MAGIC_BOOT

/* --- Simboli dal linker di Core1 --- */
extern unsigned long __stack_top__;
extern unsigned long __image_base__;
extern unsigned long __data_start__, __data_end__, __data_load__;
extern unsigned long __bss_start__,  __bss_end__;

/* --- Entry C e vettori --- */
extern void core1_main(void);
extern void __core1_vectors(void);

/* =========================================================
 * Helpers
 * =======================================================*/
static inline void cpsid_if(void) { __asm__ volatile("cpsid if" ::: "memory"); }

/* Imposta low vectors (SCTLR.V=0) e VBAR alla base dei vettori */
static inline void set_low_vectors_and_vbar(void *base)
{
    uint32_t r;
    __asm__ volatile(
        /* SCTLR.V = 0 (low vectors) */
        "mrc p15,0,%0,c1,c0,0 \n\t"
        "bic %0,%0,#(1<<13)   \n\t"   /* clear V bit */
        "mcr p15,0,%0,c1,c0,0 \n\t"
        "isb                  \n\t"
        /* VBAR = base */
        "mcr p15,0,%1,c12,c0,0\n\t"
        "isb                  \n\t"
        : "=&r"(r) : "r"(base) : "memory");
}

/* Invalida I-cache (utile dopo download AXF in debug) */
static inline void icache_invalidate_all(void)
{
    __asm__ volatile ("mcr p15,0,%0,c7,c5,0" :: "r"(0) : "memory"); /* ICIALLU */
    __asm__ volatile ("isb");
}

/* Inizializza TUTTI gli stack delle modalità bancate (SVC/IRQ/FIQ/ABT/UND/SYS) */
static inline void init_all_mode_stacks(uintptr_t sp_top)
{
    const uint32_t gap = 0x400;   /* 1 KiB per banca (aumenta se serve) */
    __asm__ volatile(
        /* Salva CPSR corrente (presumibilmente SVC) in r12 */
        "mrs    r12, cpsr                \n\t"

        /* SVC: SP = top */
        "mov    sp, %0                   \n\t"

        /* IRQ: SP = top - 1*gap */
        "bic    r1, r12, #0x1f           \n\t"
        "orr    r1, r1,  #0x12           \n\t" /* mode IRQ */
        "msr    cpsr_c, r1               \n\t"
        "sub    sp, %0, %1               \n\t"

        /* FIQ: SP = top - 2*gap */
        "bic    r1, r12, #0x1f           \n\t"
        "orr    r1, r1,  #0x11           \n\t" /* mode FIQ */
        "msr    cpsr_c, r1               \n\t"
        "sub    sp, %0, %1, lsl #1       \n\t"

        /* ABT: SP = top - 3*gap */
        "bic    r1, r12, #0x1f           \n\t"
        "orr    r1, r1,  #0x17           \n\t" /* mode ABT */
        "msr    cpsr_c, r1               \n\t"
        "sub    sp, %0, %1, lsl #1       \n\t" /* 2*gap */
        "sub    sp, sp, %1               \n\t" /* +gap => 3*gap */

        /* UND: SP = top - 4*gap */
        "bic    r1, r12, #0x1f           \n\t"
        "orr    r1, r1,  #0x1b           \n\t" /* mode UND */
        "msr    cpsr_c, r1               \n\t"
        "sub    sp, %0, %1, lsl #2       \n\t"

        /* SYS: SP = top - 5*gap */
        "bic    r1, r12, #0x1f           \n\t"
        "orr    r1, r1,  #0x1f           \n\t" /* mode SYS */
        "msr    cpsr_c, r1               \n\t"
        "sub    sp, %0, %1, lsl #2       \n\t" /* 4*gap */
        "sub    sp, sp, %1               \n\t" /* +gap => 5*gap */

        /* Ritorna a SVC */
        "msr    cpsr_c, r12              \n\t"
        :
        : "r"(sp_top), "r"(gap)
        : "r1", "r12", "memory");
}

/* Lettura mailbox CPU1 start */
static inline uint32_t mb_read_cpu1start(void) { return *core1_startaddr_mailbox(); }

/* Entry valido se è in DDR privata Core1 e word aligned */
static inline bool addr_is_in_ddr(uint32_t a)
{
    return (a >= 0x20000000u) && (a < 0x3F000000u) && ((a & 3u) == 0u);
}

/* =========================================================
 * Startup C (dual-path: orchestrato da Core0 / autonomo per debug)
 * =======================================================*/
static void __attribute__((used, section(".text.startup")))
core1_startup_body(void)
{
    /* 0) Entriamo “sterili”: IRQ/FIQ off, vettori subito */
    cpsid_if();
    set_low_vectors_and_vbar((void *)__core1_vectors);

    /* 1) Inizializza tutti gli stack bancati in DDR */
    init_all_mode_stacks((uintptr_t)&__stack_top__);

    /* 2) Percorso orchestrato da Core0? (SHM magic + mailbox valido diverso dalla nostra base) */
    const uint32_t mb = mb_read_cpu1start();
    const bool shm_ok = (SHM_CTRL && (SHM_CTRL->magic == SHM_MAGIC_BOOT));
    if (shm_ok && addr_is_in_ddr(mb) && (mb != (uint32_t)(uintptr_t)&__image_base__))
    {
        void (*entry)(void) = (void (*)(void))(uintptr_t)mb;
        entry(); /* non ritorna */
        for(;;){}
    }

    /* 3) Avvio AUTONOMO (AXF via debugger) */
    icache_invalidate_all();

    /* Copia .data */
    for (unsigned long *src = &__data_load__, *dst = &__data_start__;
         dst < &__data_end__; ++src, ++dst)
    {
        *dst = *src;
    }

    /* Azzera .bss */
    for (unsigned long *p = &__bss_start__; p < &__bss_end__; ++p)
    {
        *p = 0UL;
    }

    /* 4) Vai al main: lì configuri MMU/GIC/Timer e poi riabiliti IRQ */
    core1_main();

    for(;;){}
}

/* =========================================================
 * Reset handler: PRIMO simbolo nell'immagine (vedi linker)
 * =======================================================*/
__attribute__((naked)) void __vect_pabt_handler(void)
{
    __asm__ volatile(
        // Leggi IFSR per capire il motivo del Prefetch Abort
        "mrc p15,0,r0,c5,c0,1    \n"   // r0 = IFSR
        "ands r0, r0, #0xF       \n"   // status[3:0]
        "beq 1f                  \n"   // (0) unlikely, ma gestisci comunque

        // Caso comune BKPT su A9: molti tool lo segnalano come PABT “debug”
        // Qui NON si tenta di ritornare all’istruzione; lasciamo che il debugger prenda il controllo.
        "b   2f                  \n"

        "1:                      \n"
        // Altri PABT: resta in loop (debugger può ispezionare)
        "b   2f                  \n"

        "2:                      \n"
        "b   2b                  \n"
    );
}

void __attribute__((naked, section(".text.startup._start_core1")))
_start_core1(void)
{
    __asm__ volatile(
        "ldr sp, =__stack_top__ \n\t"
        "b   core1_startup_body \n\t"
        ::: "memory");
}

/* Alias compatibilità HWLIB */
void _socfpga_main(void) __attribute__((weak, alias("_start_core1")));

