#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "alt_mmu.h"          // HWLIB MMU
#include "socal/socal.h"      // alt_read_word/alt_write_word (se ti servono)

// Indirizzi della tua mappa (già definiti altrove? Altrimenti definiscili qui):
#define BANK_COEF_SEL_PIO_ADDR    	0xFF200130u   // HPS->FPGA (1 bit): seleziona il banco letto dall'FPGA
#define BANK_PULSE_SEL_PIO_ADDR    	0xFF200170u   // HPS->FPGA (1 bit): seleziona il banco letto dall'FPGA

#define F2H_IRQ0_EN_ADDR     		0xFF200000u
#define ARM_PIO_DATA_ADDR    		0xFF200010u
#define PRF_COUNT_ADDR    			0xFF200020u

#define MSGDMA0_CSR_BASE     		0xFF200040u
#define MSGDMA0_DESC_BASE    		0xFF200060u

#define MSGDMA1_CSR_BASE     		0xFF200080u
#define MSGDMA1_DESC_BASE    		0xFF2000A0u

#define MSGDMA2_CSR_BASE     		0xFF2000C0u
#define MSGDMA2_DESC_BASE    		0xFF2000E0u

#define MSGDMA3_CSR_BASE     		0xFF200100u
#define MSGDMA3_DESC_BASE    		0xFF200120u

#define MSGDMA4_CSR_BASE     		0xFF200140u
#define MSGDMA4_DESC_BASE    		0xFF200160u

#define DDR3_BASE                 	0x40000000u

// Max opzioni in DDR
#define COEF_NUM_OPTIONS_MAX       1024u
#define PULSE_NUM_OPTIONS_MAX     1024u

// Base delle aree in DDR (COEF prima, poi PULSE)
#define COEF_AREA_BASE             (DDR3_BASE)
// PULSE parte subito dopo l’area COEF (dipende da quante COEF opzioni usi e dal loro stride)
#define PULSE_AREA_BASE(ncoef, coef_stride)  (COEF_AREA_BASE + (uint32_t)(ncoef) * (uint32_t)(coef_stride))

// OCRAM dest: doppio buffer (tuo mapping)
#define COEF_DEST_BASE        		0x00000000u    // dual-port 1 MiB
#define COEF_NUM_PAIRS       		1024

#define PULSE_DEST_BASE           	0x00000000u    // dual-port 128 KiB
#define PULSE_NUM_PAIRS       		1024


// (Opzionale) Base SCU A9: tipicamente 0xFFFEC000 su Arria 10/Cyclone V
#define A9_SCU_BASE               0xFFFFC000u



// Inizializza MMU per CORE0 secondo mappa richiesta (non mappa il GB FPGA).
ALT_STATUS_CODE arm_mmu_setup_core0(void);

// Inizializza MMU per CORE1 secondo mappa richiesta (non mappa il GB FPGA).
ALT_STATUS_CODE arm_mmu_setup_core1(void);

// (Opzionale) Abilita SMP/SCU prima delle cache/MMU
void arm_enable_smp_and_scu(void);

ALT_STATUS_CODE arm_cache_set_enabled(bool enable);

// (opzionali se ti servono anche altrove)
void arm_icache_invalidate_all(void);
void arm_dcache_clean_invalidate_all(void);
void arm_cache_dump_status(void);
void arm_cache_get_status(bool *mmu_on, bool *icache_on, bool *dcache_on, bool *bp_on);
void arm_dcache_clean_invalidate_all(void);
void arm_icache_invalidate_all(void);

ALT_STATUS_CODE arm_mm_hw_init(void);
ALT_STATUS_CODE arm_core0_mm_open(void);
ALT_STATUS_CODE arm_core0_mm_close(void);
ALT_STATUS_CODE arm_core1_mm_open(void);
ALT_STATUS_CODE arm_core1_mm_close(void);
ALT_STATUS_CODE arm_mm_require_ready(void);


