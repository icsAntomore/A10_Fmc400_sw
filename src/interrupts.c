#include "alt_interrupt.h"
#include "alt_bridge_manager.h"
#include "interrupts.h"
#include "timers.h"

void gic_eoi(uint32_t irq_id)
{
    __asm__ volatile("dsb sy" ::: "memory");
    alt_write_word((void*)(uintptr_t)GIC_ICCEOIR, irq_id);  // EOI
    __asm__ volatile("isb" ::: "memory");
}

ALT_STATUS_CODE hps_global_interrupt_enable(void)
{
	ALT_STATUS_CODE status = ALT_E_SUCCESS;
	status = alt_int_global_init();
	return status;
}

ALT_STATUS_CODE hps_GIC_init(void)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    if (status == ALT_E_SUCCESS) status = alt_int_cpu_init();
    if (status == ALT_E_SUCCESS) status = alt_int_cpu_enable();
    if (status == ALT_E_SUCCESS) status = alt_int_global_enable();

    return status;
}


/* Initializes and enables the interrupt controller */
ALT_STATUS_CODE hps_core0_int_start(ALT_INT_INTERRUPT_t int_id,
                                      alt_int_callback_t callback,
                                      void *context,
                                      ALT_INT_TRIGGER_t trigger)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    /* Disabilita la sola linea mentre la configuri */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_disable(int_id);
    /* Pulisci eventuali pending “sporchi” sulla linea */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_pending_clear(int_id);
    /* Setup the interrupt specific items */
    if (status == ALT_E_SUCCESS) status = alt_int_isr_register(int_id, callback, context);

    /* Edge-triggered */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_trigger_set(int_id, trigger);
    /* set priorità */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_priority_set(int_id, 0x80);


    if ((status == ALT_E_SUCCESS) && (int_id >= 32)) {
        int target = 0x1; /* 1 = CPU0, 2=CPU1 */
        status = alt_int_dist_target_set(int_id, target);
    }

    /* Enable the distributor, CPU, and global interrupt */
    if (status == ALT_E_SUCCESS) status = alt_int_dist_enable(int_id);
    /*if (status == ALT_E_SUCCESS) status = alt_int_cpu_enable();
    if (status == ALT_E_SUCCESS) status = alt_int_global_enable();*/

    return status;
}

void hps_core0_int_stop(ALT_INT_INTERRUPT_t int_id)
{
    /* Disable the global, CPU, and distributor interrupt */

    alt_int_global_disable();
    alt_int_cpu_disable();
    alt_int_dist_disable(int_id);

    alt_int_isr_unregister(int_id); /* Unregister the ISR. */

    /* Uninitialize the CPU and global data structures. */

    alt_int_cpu_uninit();
    alt_int_global_uninit();
}



/* Initializes and enables the interrupt controller */
ALT_STATUS_CODE hps_core1_int_start(ALT_INT_INTERRUPT_t int_id,
                                      alt_int_callback_t callback,
                                      void *context,
                                      ALT_INT_TRIGGER_t trigger)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    // CPU interface: accetta tutto, nessun pre-split, enable IF
	alt_write_word((void*)(uintptr_t)GICC_PMR, 0xFF);
	alt_write_word((void*)(uintptr_t)GICC_BPR, 0);
	alt_write_word((void*)(uintptr_t)GICC_CTLR, 0x1);
     __asm__ volatile("dsb sy; isb");

     uint32_t bitpair = (int_id % 16u) * 2u;
     uint32_t v = alt_read_word((void*)(uintptr_t)GICD_ICDICFR1);
     v &= ~(3u << bitpair);                 /* default LEVEL = 00b */

     switch (trigger) {
     	 case ALT_INT_TRIGGER_LEVEL:
     		 /* già 00b */
     		 break;
     	 case ALT_INT_TRIGGER_EDGE:
     		 v |= (2u << bitpair);          /* EDGE = 10b */
     		 break;
     	 case ALT_INT_TRIGGER_SOFTWARE:
     		 /* valido SOLO per SGI (0..15); non c’è nulla da settare in ICFGR */
     		 if (int_id > 15u) return -2;
     		 break;
     	 case ALT_INT_TRIGGER_AUTODETECT:
     		 /* Se vuoi “auto”: tipicamente usi LEVEL di default */
     		 break;
     	 default:
     		 return -3;
	}
    alt_write_word((void*)(uintptr_t)GICD_ICDICFR1, v);
    alt_write_word((void*)(uintptr_t)GICD_ICDISER1, (1u << int_id)); //abilita la linea di interrupt

    __asm__ volatile("dsb sy; isb");

    return status;
}


void core1_irq_handler_c(void) //vedi core1_vectors.S
{
    uint32_t iar   = alt_read_word((void*)(uintptr_t)GICC_IAR);
    uint32_t intid = (iar & 0x3FFu);  // 0..1023

    // Spurious?
    if (intid >= 1020u) {
    	gic_eoi(iar);
        return;
    }

    switch(intid) {
    	case ALT_INT_INTERRUPT_PPI_TIMER_PRIVATE:
    		core1_timer_int_callback(iar, NULL);
		break;
    }


    // EOI: sempre con l’IAR originale
    gic_eoi(iar);
}
