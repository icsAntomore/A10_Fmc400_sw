#ifndef CORE_BOOT_MAILBOX_H
#define CORE_BOOT_MAILBOX_H

#include <stdint.h>

/*
 * Registro ROMCODE "CPU1 start address" del System Manager (Arria 10).
 * Core0 lo programma con l'entry point fisico dell'immagine da eseguire
 * su CPU1; Core1 lo può rileggere per sapere dove saltare.
 */
#define SYSMGR_ROM_BASE            0xFFD06200u
#define A10_ROM_CPU1START_ADDR     (SYSMGR_ROM_BASE + 0x08u)

static inline volatile uint32_t *core1_startaddr_mailbox(void)
{
    return (volatile uint32_t *)A10_ROM_CPU1START_ADDR;
}

#endif /* CORE_BOOT_MAILBOX_H */
