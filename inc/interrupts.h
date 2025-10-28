#include "alt_fpga_manager.h"
#include "alt_interrupt.h"


// Basi GIC su A9 HPS (Arria 10/Cyclone V)
#define GIC_CPU_IF_BASE   0xFFFFC100u   // CPU interface base
#define GIC_DIST_IF_BASE  0xFFFFD000u  /* Distributor */


#define GIC_ICCEOIR       (GIC_CPU_IF_BASE + 0x10u)  // End Of Interrupt
#define GICC_IAR          (GIC_CPU_IF_BASE + 0x00C)
#define GICC_PMR          (GIC_CPU_IF_BASE + 0x004)
#define GICC_BPR          (GIC_CPU_IF_BASE + 0x008)
#define GICC_CTLR         (GIC_CPU_IF_BASE + 0x000)
#define GICD_ICDICFR1     (GIC_DIST_IF_BASE + 0xC04)  /* cfg 16..31 (PPI), 2 bit per ID*/
#define GICD_ICDISER1     (GIC_DIST_IF_BASE + 0x100)  /* 0..31 (SGI+PPI, banked per CPU) */


/* Initializes and enables the interrupt controller.*/
void gic_eoi(uint32_t irq_id);

ALT_STATUS_CODE hps_global_interrupt_enable(void);
ALT_STATUS_CODE hps_GIC_init(void);

ALT_STATUS_CODE hps_core0_int_start(ALT_INT_INTERRUPT_t int_id,
                                       alt_int_callback_t callback,
                                       void *context,
                                       ALT_INT_TRIGGER_t trigger);

void hps_core0_int_stop(ALT_INT_INTERRUPT_t int_id);

ALT_STATUS_CODE hps_core1_int_start(ALT_INT_INTERRUPT_t int_id,
                                       alt_int_callback_t callback,
                                       void *context,
                                       ALT_INT_TRIGGER_t trigger);
