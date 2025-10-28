/*****************************************************************************
*
* ICS Technologies SPA. All Rights Reserved.
* 
*****************************************************************************/
#include <string.h>
#include <inttypes.h>
#include "alt_printf.h"
#include <stdbool.h>
#include "alt_clock_manager.h"
#include "alt_timers.h"
#include "alt_interrupt.h"
#include "alt_watchdog.h"
#include "alt_fpga_manager.h"
#include "interrupts.h"
#include "timers.h"
#include "arm_pio.h"
#include "schedule.h"
#include "f2h_interrupts.h"
#include "uart_stdio.h"
#include "arm_mem_regions.h"
#include "msgdma.h"
#include "dma_layout.h"
#include "socal/socal.h"
#include "socal/alt_rstmgr.h"
#include "qspi.h"

extern volatile uint32_t *g_arm_pio_data;
extern volatile uint32_t *g_arm_msgdma0_csr;
extern volatile uint32_t *g_arm_msgdma1_csr;
extern volatile uint32_t *g_arm_msgdma2_csr;
extern volatile uint32_t *g_arm_msgdma3_csr;
extern volatile uint32_t *g_arm_msgdma4_csr;
extern volatile uint32_t *g_arm_f2h_irq0_en;


int main(int argc, char** argv)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    /* Disable watchdogs */
    alt_wdog_stop(ALT_WDOG0);
    alt_wdog_stop(ALT_WDOG1);


    // subito all’inizio del main
    arm_cache_set_enabled(false);   // spegne I/D cache + branch predictor
    arm_icache_invalidate_all();
    arm_dcache_clean_invalidate_all();


    if (status == ALT_E_SUCCESS) status = arm_mmu_setup_core0();

    if (status == ALT_E_SUCCESS) status = arm_core0_mm_open();
    if (status == ALT_E_SUCCESS) status = arm_pio_write(g_arm_pio_data,0x00000000);        // chiude canale output
    if (status == ALT_E_SUCCESS) status = arm_pio_write(g_arm_pio_data,0x80000000);        // apre canale output

    /* Inizializza GIC una sola volta */
    if (status == ALT_E_SUCCESS) status = hps_global_interrupt_enable();
    if (status == ALT_E_SUCCESS) status = hps_GIC_init();

    /* ---- QUI aggiungi le priorità ---- */
    if (status == ALT_E_SUCCESS) {
        // consenti tutte le priorità e nesting
        alt_int_cpu_priority_mask_set(0xFF);   // PMR: non filtra nulla
        alt_int_cpu_binary_point_set(0);       // permette preemption annidata

        // mSGDMA più urgente del trigger
        alt_int_dist_priority_set(IRQ_ID_F2H0_5, 0x20); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_4, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_3, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_2, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_1, 0x30); // DMA
        alt_int_dist_priority_set(IRQ_ID_F2H0_0, 0x60); // trigger
        alt_int_dist_priority_set(ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE, 0xA0);

        // per sicurezza: indirizza tutti su CPU0
        alt_int_dist_target_set(IRQ_ID_F2H0_5, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_4, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_3, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_2, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_1, 0x1);
        alt_int_dist_target_set(IRQ_ID_F2H0_0, 0x1);
    }

    if (status == ALT_E_SUCCESS) {
        status = hps_core0_int_start(ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE,
        							core0_timer_int_callback,
									NULL,
									ALT_INT_TRIGGER_LEVEL); /* Start the interrupt system */
    }

    if (status == ALT_E_SUCCESS) {
    	status = hps_core0_int_start(IRQ_ID_F2H0_0,
    								fpga_f2h0_isr,
									NULL,
									ALT_INT_TRIGGER_EDGE);
    }

    /* Start the timer system */
    if (status == ALT_E_SUCCESS) status = hps_timer_start(ALT_GPT_CPU_PRIVATE_TMR, 1);

    if (status == ALT_E_SUCCESS) status = uart_stdio_init_uart1(115200);


    printf("\nFMC400 Start!");
    if (status == ALT_E_SUCCESS)
    	printf("\nFMC400 Init is OK");
    else
    	printf("\nFMC400 Init is FAIL");

    // (opz.) stampa stato per verifica
    arm_cache_dump_status();        // deve mostrare I=0 D=0 BP=0
    //change_pulse();

    /* 1 riceve dati da CPU100 PULSE e REF da mettere in memoria
     * 2 finito questo passaggio abilita il trasferimento DMA dei coefficienti
     * 3 ci deve essere una funzione che gira di continuo che sente la variazione dei PULSE e di conseguenza
     *   dei REF sia quando li riceve ex novo, sia quando l'operatore vuole cambiare la configurazione da trasmettere
     */
    start_mSGDMA(g_arm_msgdma0_csr,0);
    start_mSGDMA(g_arm_msgdma1_csr,1);
    start_mSGDMA(g_arm_msgdma2_csr,2);
    start_mSGDMA(g_arm_msgdma3_csr,3);
    start_mSGDMA(g_arm_msgdma4_csr,4);

    arm_pio_write(g_arm_f2h_irq0_en,1); //enable interrupt del trigger!!! bisogna farlo dopo aver inizializzato MSGDMA

    if (status == ALT_E_SUCCESS) {
    	hps_core0_int_start(IRQ_ID_F2H0_1,
    						sgdma0_int_callback,
							NULL,
							ALT_INT_TRIGGER_LEVEL);
    }

    if (status == ALT_E_SUCCESS) {
    	hps_core0_int_start(IRQ_ID_F2H0_2,
        					sgdma1_int_callback,
    						NULL,
    						ALT_INT_TRIGGER_LEVEL);
	}

    if (status == ALT_E_SUCCESS) {
    	hps_core0_int_start(IRQ_ID_F2H0_3,
        					sgdma2_int_callback,
    						NULL,
    						ALT_INT_TRIGGER_LEVEL);
    }

    if (status == ALT_E_SUCCESS) {
    	hps_core0_int_start(IRQ_ID_F2H0_4,
        					sgdma3_int_callback,
    						NULL,
    						ALT_INT_TRIGGER_LEVEL);
    }

    if (status == ALT_E_SUCCESS) {
    	hps_core0_int_start(IRQ_ID_F2H0_5,
    						sgdma4_int_callback,
							NULL,
							ALT_INT_TRIGGER_LEVEL);
    }

    core1_on();
	sched_insert(CORE0,SCHED_PERIODIC,ledsys_core0,300);
	sched_insert(CORE0,SCHED_ONETIME,check_core1,1000);
	sched_insert(CORE0,SCHED_PERIODIC,change_pulse,5000);

    if (status == ALT_E_SUCCESS) {
        while (1) {
        	sched_manager(CORE0);
        } /* Wait for the timer to be called X times. */
    }
    return 0;
}
