// MMU setup minimale per Arria 10 HPS (Cortex-A9), con diagnostica integrata.
// - DDR 0x00000000–0x3FFFFFFF : Normal WBWA, Shareable, XN=off (eseguibile) [1 GB]
// - HPS/Bridge 0xFF000000–0xFFFFFFFF : Device, Shareable, XN=on [16 MB]
// - 0x40000000–0x7FFFFFFF : NON mappato (1 GB riservato alla FPGA)
// VA == PA. Solo granulosità da 1 MB (Section). Nessuna sotto-mappa sub-MB.

#include "arm_mem_regions.h"
#include "alt_bridge_manager.h"
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include "uart_stdio.h"
#include "alt_printf.h"
#include "shared_ipc.h"
#include "alt_mmu.h"


#ifndef A9_SCU_BASE
#define A9_SCU_BASE 0xFFFFC000u
#endif

// ---------------------------
// Helpers CP15 / barriere
// ---------------------------
static inline void dmb(void){ __asm__ volatile("dmb sy" ::: "memory"); }
static inline void dsb(void){ __asm__ volatile("dsb sy" ::: "memory"); }
static inline void isb(void){ __asm__ volatile("isb" ::: "memory"); }


// Disabilita MMU/I/D/BP e invalida TLB (presume IRQ/FIQ OFF e VBAR già ok)
static inline void a9_sanitize_before_mmu(void)
{
	/* Presupposto: IRQ/FIQ disabilitati (cpsid if) e VBAR già puntato ai tuoi vettori */
	uint32_t sctlr, new_sctlr;

	__asm__ volatile("mrc p15,0,%0,c1,c0,0" : "=r"(sctlr));

	// NON toccare M (bit0). Spegni solo: C (D-cache, bit2), Z (branch predictor, bit11), I (I-cache, bit12).
	new_sctlr = sctlr & ~((1u << 12) | (1u << 11) | (1u << 2));
	if (new_sctlr != sctlr) {
		__asm__ volatile("mcr p15,0,%0,c1,c0,0" :: "r"(new_sctlr));
		__asm__ volatile("dsb sy\nisb");
	}

	// Invalida I-cache tutta (ICIALLU) e Branch Predictor (BPIALL)
	__asm__ volatile("mcr p15,0,%0,c7,c5,0" :: "r"(0) : "memory");  // ICIALLU
	__asm__ volatile("mcr p15,0,%0,c7,c5,6" :: "r"(0) : "memory");  // BPIALL
	__asm__ volatile("dsb sy\nisb");

	// Invalida TLB unificati (TLBIALL). Non è necessario che M sia 0 per farla.
	__asm__ volatile("mcr p15,0,%0,c8,c7,0" :: "r"(0) : "memory");  // TLBIALL
	__asm__ volatile("dsb sy\nisb");
}

// Abilita SMP + SCU (prima di accendere cache/MMU)
void arm_enable_smp_and_scu(void)
{
    uint32_t actlr;
    __asm__ volatile("mrc p15,0,%0,c1,c0,1":"=r"(actlr));
    actlr |= (1u<<6); // ACTLR.SMP=1
    __asm__ volatile("mcr p15,0,%0,c1,c0,1"::"r"(actlr));
    dsb(); isb();

   /* volatile uint32_t *SCU_CTRL = (volatile uint32_t *)(uintptr_t)(A9_SCU_BASE + 0x00u);
    *SCU_CTRL |= 1u; // enable SCU
    dsb(); isb();*/

    /*
     * Alcuni SoC (incluso Arria 10) generano un abort quando il core secondario
     * prova ad accedere ai registri privati della SCU.  In tal caso occorre
     * abilitare la SCU esclusivamente dal core 0, mentre è comunque necessario
     * impostare il bit SMP sugli altri core.
    */
    uint32_t mpidr;
    __asm__ volatile("mrc p15,0,%0,c0,c0,5" : "=r"(mpidr));
    if ((mpidr & 0x3u) == 0u) {
    	volatile uint32_t *SCU_CTRL = (volatile uint32_t *)(uintptr_t)(A9_SCU_BASE + 0x00u);
        uint32_t val = *SCU_CTRL;
        val |= 1u; // enable SCU
        *SCU_CTRL = val;
        dsb(); isb();
    }
}

// ---------------------------
// MMU: storage per le tabelle
// ---------------------------
/* 64 KiB bastano per 4 regioni (L1 + alcune L2) */
static uint8_t s_ttb_storage[64 * 1024] __attribute__((aligned(16384)));

typedef struct {
    uint8_t *base;
    size_t   size;
    size_t   off;
    int      first_done;   /* 0 = prossima alloc è L1 (16KiB); 1 = L2 (1KiB) */
} mmu_pool_t;

static mmu_pool_t s_mmu_pool = {
    .base = s_ttb_storage,
    .size = sizeof(s_ttb_storage),
    .off  = 0,
    .first_done = 0
};

static inline uintptr_t align_up(uintptr_t a, size_t align)
{
    return (a + (align - 1)) & ~(uintptr_t)(align - 1);
}

/* ===== Allocator con firma richiesta da HWLIB =====
   Prima alloc: 16 KiB; successive: 1 KiB */
static void *mmu_aux_alloc_v1(const unsigned int bytes, void *user)
{
    mmu_pool_t *p = (mmu_pool_t *)user;
    if (!p || !p->base) return NULL;

    const size_t align = p->first_done ? 1024u : 16384u;

    uintptr_t cur     = (uintptr_t)p->base + p->off;
    uintptr_t aligned = align_up(cur, align);
    size_t new_off    = (size_t)(aligned - (uintptr_t)p->base) + (size_t)bytes;

    if (new_off > p->size) return NULL;

    p->off = new_off;
    p->first_done = 1;  /* dalla prossima in poi L2 */
    return (void *)aligned;
}


// VA space per CORE1: Shared low DDR + DDR privata core1 + SHM non-cache + periferiche HPS.
static ALT_STATUS_CODE create_va_space_core1_ddr(uint32_t **ttb_out)
{
    ALT_STATUS_CODE s = alt_mmu_init();
    if (s != ALT_E_SUCCESS) return s;

    /* reset pool ogni volta (utile in debug) */
    s_mmu_pool.off = 0;
    s_mmu_pool.first_done = 0;

    ALT_MMU_MEM_REGION_t regions[] = {
        /* 0x0000_0000 – 0x00FF_FFFF (16 MiB), WBWA, shareable */
        {
            .va         = (void*)0x00000000u,
            .pa         = (void*)0x00000000u,
            .size       = 0x01000000u,
            .access     = ALT_MMU_AP_FULL_ACCESS,
            .attributes = ALT_MMU_ATTR_WBA,
            .shareable  = ALT_MMU_TTB_S_SHAREABLE,
            .execute    = ALT_MMU_TTB_XN_DISABLE,
            .security   = ALT_MMU_TTB_NS_SECURE
        },
        /* 0x2000_0000 – 0x3EFF_FFFF (496 MiB), WBWA, non-shareable */
        {
            .va         = (void*)0x20000000u,
            .pa         = (void*)0x20000000u,
            .size       = 0x1F000000u,
            .access     = ALT_MMU_AP_FULL_ACCESS,
            .attributes = ALT_MMU_ATTR_WBA,
            .shareable  = ALT_MMU_TTB_S_NON_SHAREABLE,
            .execute    = ALT_MMU_TTB_XN_DISABLE,
            .security   = ALT_MMU_TTB_NS_SECURE
        },
        /* SHM 0x3F00_0000 – 0x3FFF_FFFF (16 MiB), Device */
        {
            .va         = (void*)SHM_BASE,
            .pa         = (void*)SHM_BASE,
            .size       = SHM_SIZE,
            .access     = ALT_MMU_AP_FULL_ACCESS,
            .attributes = ALT_MMU_ATTR_DEVICE,
            .shareable  = ALT_MMU_TTB_S_SHAREABLE,
            .execute    = ALT_MMU_TTB_XN_DISABLE, /* o ENABLE se vuoi NX */
            .security   = ALT_MMU_TTB_NS_SECURE
        },
        /* Periferiche 0xFF00_0000 – 0xFFFF_FFFF (16 MiB), Device, XN */
        {
            .va         = (void*)0xFF000000u,
            .pa         = (void*)0xFF000000u,
            .size       = 0x01000000u,
            .access     = ALT_MMU_AP_FULL_ACCESS,
            .attributes = ALT_MMU_ATTR_DEVICE,
            .shareable  = ALT_MMU_TTB_S_SHAREABLE,
            .execute    = ALT_MMU_TTB_XN_ENABLE,
            .security   = ALT_MMU_TTB_NS_SECURE
        }
    };

    const size_t region_count = sizeof(regions) / sizeof(regions[0]);

    /* Verifica spazio richiesto (facoltativo ma utile in debug) */
    size_t need = alt_mmu_va_space_storage_required(regions, region_count);
    if (need > sizeof(s_ttb_storage)) {
        /* Se non basta, aumenta s_ttb_storage sopra */
        return ALT_E_BAD_ARG;
    }

    uint32_t *ttb1 = NULL;

    __asm__ volatile("dsb sy\nisb");
    s = alt_mmu_va_space_create(&ttb1,
                                regions, region_count,
                                /* ttb_alloc     */ mmu_aux_alloc_v1,
                                /* ttb_alloc_usr */ &s_mmu_pool);
    if (s != ALT_E_SUCCESS) return s;

    __asm__ volatile("dsb sy\nisb");
    s = alt_mmu_va_space_enable(ttb1);
    if (s != ALT_E_SUCCESS) return s;

    __asm__ volatile("dsb sy\nisb");

    if (ttb_out) *ttb_out = ttb1;
    return ALT_E_SUCCESS;
}

// Core0 → mappa [C0]
ALT_STATUS_CODE arm_mmu_setup_core0(void)
{
	extern char _vectors;

	    // IRQ/FIQ off durante l’early init
	    __asm__ volatile("cpsid if");

	    // punta VBAR ai vettori in OCRAM (necessario a prescindere)
	    __asm__ volatile("mcr p15,0,%0,c12,c0,0" :: "r"(&_vectors) : "memory");
	    __asm__ volatile("isb");

	    // assicurati che MMU/I/D/BP siano spenti (profilo minimal)
	    uint32_t sctlr;
	    __asm__ volatile("mrc p15,0,%0,c1,c0,0":"=r"(sctlr));
	    sctlr &= ~((1u<<0)|(1u<<2)|(1u<<12)|(1u<<11)); // M=0,C=0,I=0,Z=0
	    __asm__ volatile("mcr p15,0,%0,c1,c0,0"::"r"(sctlr));
	    __asm__ volatile("dsb sy; isb");

	    // tieni gli interrupt mascherati; li riapri tu dopo aver init GIC/ISR
	    return ALT_E_SUCCESS;
}

// Core1 → mappa [C1]
ALT_STATUS_CODE arm_mmu_setup_core1(void)
{
    //a9_sanitize_before_mmu(); // commento altrimenti il debug con ADS non funziona
    arm_enable_smp_and_scu();

    __asm__ volatile("dsb sy\nisb");

    uint32_t *ttb = NULL;
    int s;
    s=create_va_space_core1_ddr(&ttb);

    __asm__ volatile("dsb sy\nisb");
    return s;
}


// ===== Cache control (L1) =====

// Invalida tutta la I-Cache
void arm_icache_invalidate_all(void)
{
    uint32_t zero = 0;
    __asm__ volatile("mcr p15, 0, %0, c7, c5, 0" :: "r"(zero)); // ICIALLU
    dsb(); isb();
}

// Clean+Invalidate di tutta la D-Cache (per set/way)
void arm_dcache_clean_invalidate_all(void)
{
    uint32_t ccsidr, sets, ways, set, way, line_shift;

    // Seleziona L1 Data/Unified (livello 0, Data=0) in CSSELR
    uint32_t csselr = 0; // Level=0, InD=0 (Data/unified)
    __asm__ volatile("mcr p15, 2, %0, c0, c0, 0" :: "r"(csselr));
    isb();

    // Leggi CCSIDR
    __asm__ volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(ccsidr));

    // Calcoli da CCSIDR
    line_shift = ((ccsidr & 0x7u) + 4u);          // log2(bytes_per_line)
    sets       = ((ccsidr >> 13) & 0x7FFFu);      // NumSets - 1
    ways       = ((ccsidr >> 3)  & 0x3FFu);       // Associativity - 1

    for (way = ways; ; way--) {
        for (set = sets; ; set--) {
            uint32_t sw = (way << 30) | (set << line_shift); // DCCISW format
            __asm__ volatile("mcr p15, 0, %0, c7, c14, 2" :: "r"(sw)); // DCCISW
            if (set == 0) break;
        }
        if (way == 0) break;
    }
    dsb(); // assicurati che la writeback sia completata
}

// Abilita/Disabilita Branch Predictor, I-Cache, D-Cache
static inline void arm_set_cache_bits(bool enable)
{
    uint32_t sctlr;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(sctlr));
    if (enable) {
        sctlr |=  (1u<<11);   // Z = Branch predictor
        sctlr |=  (1u<<12);   // I = I-Cache
        sctlr |=  (1u<<2);    // C = D-Cache
    } else {
        sctlr &= ~(1u<<2);    // C = 0 (D-Cache OFF)
        sctlr &= ~(1u<<12);   // I = 0 (I-Cache OFF)
        sctlr &= ~(1u<<11);   // Z = 0 (BP OFF)
    }
    __asm__ volatile("mcr p15, 0, %0, c1, c0, 0" :: "r"(sctlr));
    dsb(); isb();
}

// API semplice per il main
ALT_STATUS_CODE arm_cache_set_enabled(bool enable)
{
    if (enable) {
        // Prima invalida tutto, poi accendi
        arm_icache_invalidate_all();
        arm_dcache_clean_invalidate_all(); // safe anche se non “strettamente” necessaria in enable
        // Invalida predittore
        uint32_t zero = 0;
        __asm__ volatile("mcr p15, 0, %0, c7, c5, 6" :: "r"(zero)); // BPIALL
        dsb(); isb();

        arm_set_cache_bits(true);
    } else {
        // Prima pulisci+invalida D, poi spegni
        arm_dcache_clean_invalidate_all();
        // Invalida I-cache e predittore per sicurezza
        arm_icache_invalidate_all();
        uint32_t zero = 0;
        __asm__ volatile("mcr p15, 0, %0, c7, c5, 6" :: "r"(zero)); // BPIALL
        dsb(); isb();

        arm_set_cache_bits(false);
    }
    return ALT_E_SUCCESS;
}

// Prototipi (in arm_mem_regions.h se vuoi usarle altrove)
void arm_cache_get_status(bool *mmu_on, bool *icache_on, bool *dcache_on, bool *bp_on);
void arm_cache_dump_status(void);

static inline uint32_t arm_read_sctlr(void)
{
    uint32_t s;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(s));
    return s;
}

void arm_cache_get_status(bool *mmu_on, bool *icache_on, bool *dcache_on, bool *bp_on)
{
    uint32_t s = arm_read_sctlr();
    if (mmu_on)    *mmu_on    = (s >> 0)  & 1u;
    if (dcache_on) *dcache_on = (s >> 2)  & 1u;
    if (bp_on)     *bp_on     = (s >> 11) & 1u;
    if (icache_on) *icache_on = (s >> 12) & 1u;
}

void arm_cache_dump_status(void)
{
    uint32_t s = arm_read_sctlr();
    // usa printf/alt_printf a tua scelta
    printf("\nCache Status: SCTLR=0x%08" PRIX32 "  MMU:%u  I:%u  D:%u  BP:%u",
           s, (s>>0)&1, (s>>12)&1, (s>>2)&1, (s>>11)&1);
}

bool arm_l2_is_enabled(void)
{
#ifdef L2C310_BASE
    volatile uint32_t *L2C_CTRL = (volatile uint32_t *)(uintptr_t)(L2C310_BASE + 0x100u);
    return ((*L2C_CTRL) & 1u) != 0u; // bit0 = L2 enable
#else
    return false; // sconosciuto
#endif
}

// ---------------------------
// MMIO pointers & bridge init
// ---------------------------

volatile uint32_t *g_bank_coef_sel    = 0;
volatile uint32_t *g_bank_pulse_sel   = 0;
volatile uint32_t *g_arm_pio_data     = 0;
volatile uint32_t *g_arm_msgdma0_csr  = 0;
volatile uint32_t *g_arm_msgdma0_desc = 0;
volatile uint32_t *g_arm_msgdma1_csr  = 0;
volatile uint32_t *g_arm_msgdma1_desc = 0;
volatile uint32_t *g_arm_msgdma2_csr  = 0;
volatile uint32_t *g_arm_msgdma2_desc = 0;
volatile uint32_t *g_arm_msgdma3_csr  = 0;
volatile uint32_t *g_arm_msgdma3_desc = 0;
volatile uint32_t *g_arm_msgdma4_csr  = 0;
volatile uint32_t *g_arm_msgdma4_desc = 0;
volatile uint32_t *g_arm_f2h_irq0_en  = 0;
volatile uint32_t *g_arm_prf_counter  = 0;

static bool g_bridge_ready = false;

ALT_STATUS_CODE arm_mm_require_ready(void)
{
    if (g_arm_pio_data &&
        g_arm_msgdma0_csr && g_arm_msgdma0_desc &&
        g_arm_msgdma1_csr && g_arm_msgdma1_desc)
        return ALT_E_SUCCESS;
    return ALT_E_ERROR;
}

ALT_STATUS_CODE arm_mm_hw_init(void)
{
    // LWH2F sta in 0xFF20_xxxx -> coperto dalla regione Device
    ALT_STATUS_CODE s = alt_bridge_init(ALT_BRIDGE_LWH2F);
    if (s != ALT_E_SUCCESS) return s;

    // ✭ Aggiungi anche il bridge Fabric→HPS (include F2SDRAM)
    s = alt_bridge_init(ALT_BRIDGE_F2S);
    if (s != ALT_E_SUCCESS) return s;

    g_bridge_ready = true;
    return ALT_E_SUCCESS;
}

ALT_STATUS_CODE arm_core0_mm_open(void)
{
    ALT_STATUS_CODE s = ALT_E_SUCCESS;

    if (!g_bridge_ready) {
        s = arm_mm_hw_init();
        if (s != ALT_E_SUCCESS) return s;
    }

    // VA==PA
    g_bank_coef_sel    = (volatile uint32_t*)(uintptr_t) BANK_COEF_SEL_PIO_ADDR;
    g_bank_pulse_sel   = (volatile uint32_t*)(uintptr_t) BANK_PULSE_SEL_PIO_ADDR;
    g_arm_pio_data     = (volatile uint32_t*)(uintptr_t) ARM_PIO_DATA_ADDR;
    g_arm_msgdma0_csr  = (volatile uint32_t*)(uintptr_t) MSGDMA0_CSR_BASE;
    g_arm_msgdma0_desc = (volatile uint32_t*)(uintptr_t) MSGDMA0_DESC_BASE;
    g_arm_msgdma1_csr  = (volatile uint32_t*)(uintptr_t) MSGDMA1_CSR_BASE;
    g_arm_msgdma1_desc = (volatile uint32_t*)(uintptr_t) MSGDMA1_DESC_BASE;
    g_arm_msgdma2_csr  = (volatile uint32_t*)(uintptr_t) MSGDMA2_CSR_BASE;
    g_arm_msgdma2_desc = (volatile uint32_t*)(uintptr_t) MSGDMA2_DESC_BASE;
    g_arm_msgdma3_csr  = (volatile uint32_t*)(uintptr_t) MSGDMA3_CSR_BASE;
	g_arm_msgdma3_desc = (volatile uint32_t*)(uintptr_t) MSGDMA3_DESC_BASE;
	g_arm_msgdma4_csr  = (volatile uint32_t*)(uintptr_t) MSGDMA4_CSR_BASE;
 	g_arm_msgdma4_desc = (volatile uint32_t*)(uintptr_t) MSGDMA4_DESC_BASE;
    g_arm_f2h_irq0_en  = (volatile uint32_t*)(uintptr_t) F2H_IRQ0_EN_ADDR;
    g_arm_prf_counter  = (volatile uint32_t*)(uintptr_t) PRF_COUNT_ADDR;

    return s;
}

ALT_STATUS_CODE arm_core0_mm_close(void)
{
	g_bank_coef_sel = NULL;
	g_bank_pulse_sel = NULL;
    g_arm_pio_data = NULL;
    g_arm_msgdma0_csr = NULL;
    g_arm_msgdma0_desc = NULL;
    g_arm_msgdma1_csr = NULL;
    g_arm_msgdma1_desc = NULL;
    g_arm_f2h_irq0_en = NULL;
	g_arm_prf_counter = NULL;

    return ALT_E_SUCCESS;
}


ALT_STATUS_CODE arm_core1_mm_open(void)
{
    ALT_STATUS_CODE s = ALT_E_SUCCESS;

  /*  if (!g_bridge_ready) {
        s = arm_mm_hw_init();
        if (s != ALT_E_SUCCESS) return s;
    }*/

    g_arm_pio_data     = (volatile uint32_t*)(uintptr_t) ARM_PIO_DATA_ADDR;
    __asm__ volatile("dmb sy");

    return s;
}

ALT_STATUS_CODE arm_core1_mm_close(void)
{
    g_arm_pio_data = NULL;

    return ALT_E_SUCCESS;
}
