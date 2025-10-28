// qspi_utils.c

#include <stdio.h>
#include <qspi.h>
#include "alt_printf.h"
#include "uart_stdio.h"
#include <stdint.h>
#include "shared_ipc.h"


// Se la tua HWLIB ha ECC per Arria10, abilitalo (è dichiarato nel tuo header con #if defined(soc_a10))
static ALT_STATUS_CODE qspi_init_safe(void)
{
    ALT_STATUS_CODE s;

    // 0) Assicurati che il controller sia in stato pulito
    (void)alt_qspi_uninit();   // ok anche se non era init
    (void)alt_qspi_disable();  // ok anche se era già disabilitato

    // 1) (Opzionale ma utile) rallenta la clock prima dell’init: /16 o /32
    // Se fallisce, prosegui lo stesso.
    (void)alt_qspi_baud_rate_div_set(ALT_QSPI_BAUD_DIV_16);

    // 2) Init di base del controller
    s = alt_qspi_init();
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_init() fail: %d", (int)s);
        return s;
    }

#if defined(soc_a10)
    // 3) (Solo Arria10) Avvia ECC RAM del QSPI (API dichiarata nel tuo header)
    s = alt_qspi_ecc_start();
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_ecc_start() fail: %d (continuo comunque)", (int)s);
        // non fermarti, alcune board non lo richiedono
    }
#endif

    // 4) Config di timing base molto conservativa (clock inattiva alta, ecc.)
    ALT_QSPI_TIMING_CONFIG_t tcfg = {
        .clk_phase = ALT_QSPI_CLK_PHASE_INACTIVE,
        .clk_pol   = ALT_QSPI_CLK_POLARITY_HIGH,
        .cs_da     = 2,
        .cs_dads   = 2,
        .cs_eot    = 2,
        .cs_sot    = 2,
        .rd_datacap= 1        // aggiungi 1 ciclo di capture per sicurezza
    };
    (void)alt_qspi_timing_config_set(&tcfg); // Se non supportato, ignora il ritorno

    // 5) Config dimensioni device (valori GENERICI, modificali poi per il tuo chip)
    ALT_QSPI_DEV_SIZE_CONFIG_t dsz = {
        .block_size        = 16,   // 2^16 = 64KB block (classico dei NOR)
        .page_size         = 256,  // 256B
        .addr_size         = 2,    // 3-byte addressing (0=1B,1=2B,2=3B,3=4B)
        .lower_wrprot_block= 0,
        .upper_wrprot_block= 0,
        .wrprot_enable     = false
    };
    (void)alt_qspi_device_size_config_set(&dsz); // alcune lib ignorano, ok

    // 6) Config istruzione di READ “safe”: 0x0B (Fast Read), single I/O, 8 dummy
    ALT_QSPI_DEV_INST_CONFIG_t rcfg = {
        .op_code       = 0x0B,                 // Fast Read
        .inst_type     = ALT_QSPI_MODE_SINGLE,
        .addr_xfer_type= ALT_QSPI_MODE_SINGLE,
        .data_xfer_type= ALT_QSPI_MODE_SINGLE,
        .dummy_cycles  = 8
    };
    (void)alt_qspi_device_read_config_set(&rcfg);

    // 7) Chip select singolo, nessun decode (ss_n[0] attivo)
    (void)alt_qspi_chip_select_config_set(0xE, ALT_QSPI_CS_MODE_SINGLE_SELECT);
    // mappa: cs=xxx0 => nSS[3:0]=1110 (seleziona CS0)

    // 8) Abilita il controller
    s = alt_qspi_enable();
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_enable() fail: %d", (int)s);
        return s;
    }

    // 9) Verifica idle
    if (!alt_qspi_is_idle()) {
        printf("\r\nQSPI: controller non idle dopo enable.");
        // Non è per forza un errore fatale, ma segnalo
    }

    // 10) Info utile per capire se il bus risponde
    const char* name = alt_qspi_get_friendly_name();
    if (name) printf("\r\nQSPI: device='%s'", name);
    printf("\r\nQSPI: size=%u, page=%u, multi-die=%d, die_sz=%u",
               (unsigned)alt_qspi_get_device_size(),
               (unsigned)alt_qspi_get_page_size(),
               (int)alt_qspi_is_multidie(),
               (unsigned)alt_qspi_get_die_size());

    return ALT_E_SUCCESS;
}

ALT_STATUS_CODE qspi_read(uint8_t *dst1024, size_t len, uint32_t addr)
{
    if (!dst1024) return ALT_E_BAD_ARG;

    ALT_STATUS_CODE s = qspi_init_safe();
    if (s != ALT_E_SUCCESS) return s;

    s = alt_qspi_read(dst1024, addr, len);
    if (s != ALT_E_SUCCESS) {
        printf("\r\nQSPI: alt_qspi_read() fail: %d", (int)s);
        return s;
    }

    (void)alt_qspi_uninit();
    return ALT_E_SUCCESS;
}


/********************************************************************************
 ********************* Funzioni per avvio CORE 1 da DDR *************************
 ********************************************************************************/



static ALT_STATUS_CODE qspi_copy_to_ddr(uint32_t qspi_ofs, void *ddr_dst, size_t len)
{
    ALT_STATUS_CODE s = qspi_init_safe();
    if (s != ALT_E_SUCCESS) return s;

    s = alt_qspi_read(ddr_dst, qspi_ofs, len);
    (void)alt_qspi_uninit();

    return s;
}

/* 1) Legge da QSPI -> DDR e fa flush delle cache sulla regione */
int core1_load_from_qspi_to_ddr(void)
{
    // (Consigliato) Inizializza l’area DDR per ECC
    for (volatile uint32_t *p=(uint32_t*)CORE1_DDR_BASE; p<(uint32_t*)(CORE1_DDR_BASE+CORE1_IMAGE_SIZE); ++p)
    	*p = 0u;

    ALT_STATUS_CODE s = qspi_copy_to_ddr(CORE1_QSPI_SRC, (void*)CORE1_DDR_BASE, CORE1_IMAGE_SIZE);
    if (s != ALT_E_SUCCESS) {
        alt_printf("\r\nQSPI read fail: %d", (int)s);
        return -1;
    }

    /* Flush cache L1/L2 sulla regione image */
    uintptr_t start = align_down_cache((uintptr_t)CORE1_DDR_BASE);
    uintptr_t end   = align_up_cache  ((uintptr_t)CORE1_DDR_BASE + CORE1_IMAGE_SIZE);
    alt_cache_system_clean((void *)start, (size_t)(end - start));
    alt_cache_l1_instruction_invalidate();

    dsb_isb();

    return 0;
}


/* 4) Sequenza completa: QSPI->DDR, trampoline, set start, release */
int core1_boot_from_ddr(void)
{
	/* Tieni CPU1 in reset mentre copi (facoltativo ma consigliato) */
	uint32_t r = rd32(RSTMGR_MPUMODRST_ADDR);
	wr32(RSTMGR_MPUMODRST_ADDR, r | (1u<<1));
	dsb_isb();

    if (core1_load_from_qspi_to_ddr() != 0) {
    	alt_printf("\r\nQSPI->DDR fail");
        return -1;
    }

    // 0x0 = SDRAM (L2 Address Filtering, quello che hai già)
    map_sdram_to_zero();
    // Trampolino a DDR[0] che salta a CORE1_DDR_BASE (0x20000000)
    write_trampoline_in_ddr0(CORE1_DDR_BASE);

    /* Scrivi entry fisico nel registro ROMCODE CPU1STARTADDR (A10) */
    core1_set_start_addr((uint32_t)CORE1_DDR_BASE);
    scu_enable();

    // Togli CPU1 dal reset (RSTMGR.MPUMODRST bit1 = 0)

    r = rd32(RSTMGR_MPUMODRST_ADDR);
    r &= ~(1u << 1);
    wr32(RSTMGR_MPUMODRST_ADDR, r);
    dsb_isb();

    alt_printf("\r\nCore1 boot from DDR @0x%08x (size 0x%08x)", CORE1_DDR_BASE, CORE1_IMAGE_SIZE);

    return 0;
}

void core1_on(void)
{
	//#ifdef CORE1

	    // 1) Inizializza la shared memory
		//    (MMU off + cache off su Core0 = già NC, quindi scrivi direttamente)
		SHM_CTRL->magic = SHM_MAGIC_BOOT;
		SHM_CTRL->core0_ready = 0u;
		SHM_CTRL->core1_ready = 0u;
		SHM_CTRL->trig_count  = 0u;
		SHM_CTRL->core1_timer  = 0u;
		SHM_CTRL->log_head = SHM_CTRL->log_tail = 0u;

		//if (core1_boot_from_ddr() != 0) {
		if (core1_boot_from_ddr() != 0) {
			alt_printf("\r\nCore1 boot failed");
		}
		SHM_CTRL->core0_ready = 1u;
		//
	//#endif
}

void check_core1(void)
{
	if (SHM_CTRL->core1_ready == 1) //core 1 ready
		alt_printf("\r\nWelcome Core 1!");
}
