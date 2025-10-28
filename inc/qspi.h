#include <stdint.h>
#include <stddef.h>
#include "socal/hps.h"
#include "socal/socal.h"        // alt_read_word(), alt_write_word()
#include "socal/alt_rstmgr.h"   // ALT_RSTMGR_MPUMODRST_ADDR
#include "alt_cache.h"          // alt_cache_system_clean()
#include "alt_qspi.h"           // alt_qspi_init(), alt_qspi_read(), ...
#include "core_boot_mailbox.h"

/* --- Indirizzi Arria 10 HPS --- */
#define RSTMGR_BASE              0xFFD05000u
#define RSTMGR_MPUMODRST_ADDR    (RSTMGR_BASE + 0x20u)  /* bit1=CPU1, bit0=CPU0 */
#define L3_REMAP_ADDR            0xFF800000u               /* NIC-301 GPV remap */
#define SCU_CTRL_ADDR            0xFFFFC000u               /* SCU Control Register (bit0=EN) */

#define CORE1_DDR_BASE           0x20000000u  /* tuo indirizzo fisico */
#define CORE1_IMAGE_SIZE         0x00020000u  /* es. 128 KiB */
#define CORE1_QSPI_SRC			 0x00B00000u

#define L2_AF_START   0xFFFFFC00u  // L2C-310 Address Filtering Start (A10)
#define L2_AF_END     0xFFFFFC04u  // L2C-310 Address Filtering End
#define OCRAM_BASE   0xFFE00000u
#define L3_REMAP     0xFF800000u



static inline void wr32(uint32_t a, uint32_t v){ *(volatile uint32_t*)a = v; }
static inline uint32_t rd32(uint32_t a){ return *(volatile uint32_t*)a; }
static inline void dsb_isb(void){ __asm__ volatile("dsb sy\nisb"); }

/* Allineamento a cache-line (32B) per alt_cache_* */
#ifndef ALT_CACHE_LINE_SIZE
#  define ALT_CACHE_LINE_SIZE 32u
#endif
static inline uintptr_t align_down_cache(uintptr_t a){ return a & ~(uintptr_t)(ALT_CACHE_LINE_SIZE - 1); }
static inline uintptr_t align_up_cache(uintptr_t a){ return (a + (ALT_CACHE_LINE_SIZE - 1)) & ~(uintptr_t)(ALT_CACHE_LINE_SIZE - 1); }

/* SCU on (se userai cache su entrambi i core) */
static inline void scu_enable(void){
    *(volatile uint32_t*)SCU_CTRL_ADDR |= 1u; /* bit0=enable */
    dsb_isb();
}

/* Mappa SDRAM sull'area 0x00000000..0xBFFFFFFF */
static inline void map_sdram_to_zero(void)
{
    /* Start: [31:20]=upper12bits(start); [0]=enable
       Qui Start=0x00000001 ⇒ start=0x00000000 + enable */
    ALT_CAST(volatile uint32_t*, L2_AF_START)[0] = 0x00000001u;
    /* End: [31:20]=upper12bits(end) ⇒ 0xC00 => 0xC0000000 */
    ALT_CAST(volatile uint32_t*, L2_AF_END)[0] = 0xC0000000u;
    dsb_isb();
}

/* Scrive un micro-trampolino in DDR @ 0x00000000 */
static inline void write_trampoline_in_ddr0(uint32_t entry_phys)
{
    volatile uint32_t *z = (uint32_t*)0x00000000u;
    z[0] = 0xE51FF004;        // LDR PC, [PC, #-4]
    z[1] = entry_phys;        // es. CORE1_DDR_BASE
    alt_cache_system_clean((void*)z, 32);
#if defined(ALT_CACHE_SUPPORTS_L2)
    alt_cache_l2_writeback_all();
#endif
    dsb_isb();
}

// Programma l'indirizzo di start di CPU1 nel SYSMGR (Arria 10)
static inline void core1_set_start_addr(uint32_t phys_entry)
{
    wr32(A10_ROM_CPU1START_ADDR, phys_entry);
    dsb_isb();
}


/* Funzioni */
void core1_on(void);
void check_core1(void);
int  core1_boot_from_ddr(void);
int core1_boot_minimal_probe(void);
ALT_STATUS_CODE qspi_read(uint8_t *dst1024, size_t len, uint32_t addr);

