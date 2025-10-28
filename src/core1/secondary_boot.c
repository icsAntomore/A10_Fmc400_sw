#include <stdint.h>
#include <stdbool.h>

#include "core_boot_mailbox.h"


static inline bool debugger_attached(void)
{
    uint32_t dscr;
    /*
         * Il bit[0] di DBGDSCR (HALTED) viene impostato a 1 quando il core
         * è fermo sotto il controllo del debugger.  In precedenza controllavamo
         * erroneamente il bit14 che, su Cortex-A9, non rappresenta lo stato di
         * halt: di conseguenza il rilevamento falliva e l'immagine saltava sempre
         * all'entry impostato da Core0.
         */
	 __asm__ volatile("mrc p14, 0, %0, c0, c1, 0" : "=r"(dscr));
	 return (dscr & (1u << 0)) != 0u;
}

static inline uint32_t read_mpidr(void)
{
    uint32_t mpidr;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
    return mpidr;
}

void secondary_boot_try(void)
{
	/* Se è CPU1, devi “saltare” all’entry impostato da Core0 nel registro ROMCODE */
	uint32_t mpidr = read_mpidr();
	uint32_t cpu_id = mpidr & 0x3u;   // A9: CPU ID nei bit[1:0]

	if (cpu_id != 1u) {
		/* CPU0: continua il boot normale → ritorna al CRT e poi va in main() */
		return;
	}

	if (debugger_attached()) {
		/* Avvio da debugger: lascia che il reset handler esegua l'inizializzazione completa */
			        return;
	}

	/* Leggi l’entry che Core0 ha scritto nel registro romcode_cpu1startaddr */
	uint32_t entry = *core1_startaddr_mailbox();
	if (entry == 0u) {
		/* Boot autonomo: prosegui con l'inizializzazione standard dell'immagine */
	    return;
	}

	__asm__ volatile("dsb sy; isb");
	/* Salto diretto: BX al reset handler dell’immagine Core1 */
	void (*entry_fn)(void) = (void (*)(void))(uintptr_t)entry;
	entry_fn();

	for (;;) {
		__asm__ volatile("wfe");
	}
}
