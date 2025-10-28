#include "alt_bridge_manager.h"
#include "f2h_interrupts.h"
#include "msgdma.h"
#include "arm_mem_regions.h"
#include <stdint.h>
#include "socal/socal.h"
#include <stdio.h>
#include "arm_pio.h"
#include "interrupts.h"
#include "alt_printf.h"
#include "uart_stdio.h"


extern volatile uint32_t *g_arm_msgdma0_csr;
extern volatile uint32_t *g_arm_msgdma0_desc;
extern volatile uint32_t *g_arm_msgdma1_csr;
extern volatile uint32_t *g_arm_msgdma1_desc;
extern volatile uint32_t *g_arm_msgdma2_csr;
extern volatile uint32_t *g_arm_msgdma2_desc;
extern volatile uint32_t *g_arm_msgdma3_csr;
extern volatile uint32_t *g_arm_msgdma3_desc;
extern volatile uint32_t *g_arm_msgdma4_csr;
extern volatile uint32_t *g_arm_msgdma4_desc;

void reset_mSGDMA(volatile uint32_t *msgdma_csr_add)
{
	alt_write_word((void*)(msgdma_csr_add + CSR_CONTROL_REG), (CSR_STOP_MASK | CSR_RESET_MASK));
	//while ((alt_read_word(msgdma_csr_add + CSR_STATUS_REG) & CSR_STOP_STATE_MASK) == CSR_STOP_STATE_MASK) { };
    //alt_write_word((void*)(msgdma_csr_add + CSR_CONTROL_REG), CSR_RESET_MASK);
    alt_write_word((void*)(msgdma_csr_add + CSR_CONTROL_REG), 0u);
}

ALT_STATUS_CODE init_mSGDMA(volatile uint32_t *msgdma_csr_add)
{
	uint32_t dma_status;
	uint32_t dma_control;

	dma_status = alt_read_word(msgdma_csr_add + CSR_STATUS_REG);
	if ((dma_status & (CSR_BUSY_MASK | CSR_STOP_STATE_MASK | CSR_RESET_STATE_MASK |CSR_IRQ_SET_MASK)) != 0) {
		//pr_err("initial dma status set unexpected: 0x%08X\n", dma_status);
		return ALT_E_ERROR;
		//goto bad_exit_release_mem_region;
	}

	/*controlla che siano vuoti i buffer dei comandi R/W deve essere 1 all'init*/
	if ((dma_status & CSR_DESCRIPTOR_BUFFER_EMPTY_MASK) == 0) {
		//pr_err("initial dma status cleared unexpected: 0x%08X\n", dma_status);
		return ALT_E_ERROR;
		//goto bad_exit_release_mem_region;
	}

	/*controlla che non ci sia una condizione di stop per eventuali errori, non ci sia una condizione di reset,
	non ci sia uno stop causato da un comando R/W, non ci sia stato uno stop preventivo dovuto ad un errore,
	non è attivo il global interrupt enable*/
	dma_control = alt_read_word((msgdma_csr_add + CSR_CONTROL_REG));

	if ((dma_control & (CSR_STOP_MASK |
						CSR_RESET_MASK |
						CSR_STOP_ON_ERROR_MASK |
						CSR_STOP_ON_EARLY_TERMINATION_MASK |
						CSR_GLOBAL_INTERRUPT_MASK |
						CSR_STOP_DESCRIPTORS_MASK)) != 0) {
		//pr_err("initial dma control set unexpected: 0x%08X\n", (uint32_t)dma_control);
		return ALT_E_ERROR;
		//goto bad_exit_release_mem_region;
	}
	return ALT_E_SUCCESS;
}

int msgdma_is_idle(volatile uint32_t *msgdma_csr_add)
{
    uint32_t st = alt_read_word((msgdma_csr_add + CSR_STATUS_REG));
    return ((st & CSR_BUSY_MASK) == 0) && ((st & CSR_DESCRIPTOR_BUFFER_EMPTY_MASK) != 0);
}

// ===== Init =====
void start_mSGDMA(volatile uint32_t *msgdma_csr_add, uint32_t id)
{
    reset_mSGDMA(msgdma_csr_add);
    int s = init_mSGDMA(msgdma_csr_add);

    // DOPO il reset, prima di abilitare gli IRQ
    // clear di eventuali pendenti nel DMA
    alt_write_word((void*)(msgdma_csr_add + CSR_STATUS_REG), CSR_IRQ_SET_MASK);

    // abilita GIE in RMW
	uint32_t ctrl = alt_read_word((void*)(msgdma_csr_add + CSR_CONTROL_REG));
	ctrl |= CSR_GLOBAL_INTERRUPT_MASK;
	alt_write_word((void*)(msgdma_csr_add + CSR_CONTROL_REG), ctrl);


    if (s == ALT_E_SUCCESS)
    	printf("\r\nMSGDMA %ld init is OK",id);
    else
    	printf("\r\nMSGDMA %ld init is FAIL",id);

}


void sgdma0_int_callback(uint32_t icciar, void *ctx)
{
	(void)icciar; (void)ctx;

	/* clear the IRQ state */
	alt_write_word((void *)(g_arm_msgdma0_csr + CSR_STATUS_REG),CSR_IRQ_SET_MASK);
	// EOI alla fine
	//gic_eoi(IRQ_ID_F2H0_1);
}

void sgdma1_int_callback(uint32_t icciar, void *ctx)
{
	(void)icciar; (void)ctx;

	/* clear the IRQ state */
	alt_write_word((void *)(g_arm_msgdma1_csr + CSR_STATUS_REG),CSR_IRQ_SET_MASK);
	// EOI alla fine
	//gic_eoi(IRQ_ID_F2H0_1);
}

void sgdma2_int_callback(uint32_t icciar, void *ctx)
{
	(void)icciar; (void)ctx;

	/* clear the IRQ state */
	alt_write_word((void *)(g_arm_msgdma2_csr + CSR_STATUS_REG),CSR_IRQ_SET_MASK);
	// EOI alla fine
	//gic_eoi(IRQ_ID_F2H0_1);
}

void sgdma3_int_callback(uint32_t icciar, void *ctx)
{
	(void)icciar; (void)ctx;

	/* clear the IRQ state */
	alt_write_word((void *)(g_arm_msgdma3_csr + CSR_STATUS_REG),CSR_IRQ_SET_MASK);
	// EOI alla fine
	//gic_eoi(IRQ_ID_F2H0_1);
}

void sgdma4_int_callback(uint32_t icciar, void *ctx)
{
	(void)icciar; (void)ctx;

	/* clear the IRQ state */
	alt_write_word((void *)(g_arm_msgdma4_csr + CSR_STATUS_REG),CSR_IRQ_SET_MASK);
}


