/*****************************************************************************
*
* ICS Technologies SPA. All Rights Reserved.
* 
*****************************************************************************/
#include <inttypes.h>
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
#include <stdio.h>
#include "arm_mem_regions.h"
#include "shared_ipc.h"
#include "socal/socal.h"

extern volatile uint32_t *g_arm_pio_data;

static inline void init_data(void)
{
    extern uint8_t __data_start__;
    extern uint8_t __data_end__;
    extern uint8_t __data_load__;

    const uint8_t *src = &__data_load__;
    for (uint8_t *dst = &__data_start__; dst < &__data_end__; ++dst, ++src) {
        *dst = *src;
    }
}

static inline void zero_bss(void)
{
    extern uint8_t __bss_start__;
    extern uint8_t __bss_end__;
    for (uint8_t *p = &__bss_start__; p < &__bss_end__; ++p) *p = 0;
}

void core1_main(void)
{
	ALT_STATUS_CODE status = ALT_E_SUCCESS;

	init_data();
    zero_bss();

    (void)uart_stdio_init_uart1(115200);

    // MMU di Core1 (crea le regioni: DDR WBWA + SHM NON-CACHEABLE + Device)
    if (status == ALT_E_SUCCESS) status = arm_mmu_setup_core1();
    if (status == ALT_E_SUCCESS) status = arm_core1_mm_open();

    printf("\r\n[CORE1] PIO OK, addr="); uart_stdio_write_hex32((uint32_t)g_arm_pio_data);

    if (status == ALT_E_SUCCESS) status = hps_core1_int_start(ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE,
		   	   	   	   	   	   	   	   	   	   	   	   	   	 core1_timer_int_callback,
															 NULL,
															 ALT_INT_TRIGGER_LEVEL);
    if (status == ALT_E_SUCCESS) status = hps_timer_start(ALT_GPT_CPU_PRIVATE_TMR, 1);

   __asm__ volatile("cpsie i");

    // Attendi che Core0 abbia inizializzato tutto e “aperto” l’handshake
    while (SHM_CTRL->magic != SHM_MAGIC_BOOT) { /* spin */ }
    while (SHM_CTRL->core0_ready != 1u)        { /* spin */ }

    // Saluta e dichiara “ready”

    printf("\n\rHello from HPS - Core 1. SHM @ 0x");
    (void)uart_stdio_write_hex32((uint32_t)SHM_BASE);
    printf(" - Now it's ready.");
    SHM_CTRL->core1_ready = 1u;
    __asm__ volatile("dmb sy" ::: "memory");

    sched_insert(CORE1,SCHED_PERIODIC,ledsys_core1,300);

    if (status == ALT_E_SUCCESS) {
    	while (1) {
    		SHM_CTRL->trig_count++;
    		sched_manager(CORE1);
    	}
    }
}
