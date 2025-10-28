#include "f2h_interrupts.h"
#include "alt_interrupt.h"
#include <stdio.h>
#include "socal/socal.h"
#include "msgdma.h"
#include "interrupts.h"
#include "arm_pio.h"
#include "arm_mem_regions.h"
#include "dma_layout.h"

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

static volatile uint32_t coef_bank = 0;
static volatile uint32_t coef_pair_idx = 0;

static volatile uint32_t pulse_bank = 0;
static volatile uint32_t pulse_pair_idx = 0;

static volatile uint32_t coef_len = 0;
static volatile uint32_t pulse_len = 0;

volatile uint32_t g_edges = 0;

void fpga_f2h0_isr(uint32_t icciar, void *ctx) {
	(void)icciar; (void)ctx;

	__asm__ volatile ("cpsie i");   // riapre gli IRQ durante questa ISR

	coef_bank ^= 1u; // toggle BANK_SEL → FPGA legge il banco appena riempito
	coef_bank_sel(coef_bank);
	//uint32_t bank_dst_off = (coeff_bank == 0) ? FFT_COEF_OFF_B : FFT_COEF_OFF_A;

	// size corrente (dipende dal channel)
	uint32_t fft_size = g_coef_pair_bytes[g_channel];
	uint32_t coef_dst_off = (coef_bank == 0) ? fft_size : 0u;

	coef_pair_idx = (coef_pair_idx + 1u) % COEF_NUM_PAIRS; // coppia successiva
	uint32_t coef_src = pair_coef_source_addr(coef_pair_idx);
	uint32_t coef_dst = COEF_DEST_BASE + coef_dst_off;
	coef_len = coef_len_for_channel(g_channel);  // 64k/128k/256k/512k


	pulse_bank ^= 1u; // toggle BANK_SEL → FPGA legge il banco appena riempito
	pulse_bank_sel(pulse_bank);
	//uint32_t pulse_dst_off = (pulse_bank == 0) ? PULSE_OFF_B : PULSE_OFF_A;

	// size corrente (dipende dal channel)
	uint32_t pulse_size = g_pulse_pair_bytes[g_channel];
	uint32_t pulse_dst_off = (pulse_bank == 0) ? pulse_size : 0u;

	pulse_pair_idx = (pulse_pair_idx + 1u) % PULSE_NUM_PAIRS; // coppia successiva
	uint32_t pulse_src = pair_pulse_source_addr(pulse_pair_idx);
	uint32_t pulse_dst = PULSE_DEST_BASE + pulse_dst_off;
	pulse_len = pulse_len_for_channel(g_channel);  // 1k/16k/32k/64k

	// --- attende che i due DMA siano liberi ---
	while (!msgdma_is_idle(g_arm_msgdma0_csr) ||
	       !msgdma_is_idle(g_arm_msgdma4_csr)) {
	        ; // busy-wait, assicura sequenzialità
	}

	alt_write_word((void*)(g_arm_msgdma0_desc + DESCRIPTOR_READ_ADDRESS_REG),  coef_src);
	alt_write_word((void*)(g_arm_msgdma0_desc + DESCRIPTOR_WRITE_ADDRESS_REG), coef_dst);
	alt_write_word((void*)(g_arm_msgdma0_desc + DESCRIPTOR_LENGTH_REG),        coef_len);
	alt_write_word((void*)(g_arm_msgdma0_desc + DESCRIPTOR_CONTROL_STANDARD_REG), START_MSGDMA_MASK);

	alt_write_word((void*)(g_arm_msgdma4_desc + DESCRIPTOR_READ_ADDRESS_REG),  pulse_src);
	alt_write_word((void*)(g_arm_msgdma4_desc + DESCRIPTOR_WRITE_ADDRESS_REG), pulse_dst);
	alt_write_word((void*)(g_arm_msgdma4_desc + DESCRIPTOR_LENGTH_REG),        pulse_len);
	alt_write_word((void*)(g_arm_msgdma4_desc + DESCRIPTOR_CONTROL_STANDARD_REG), START_MSGDMA_MASK);

	g_edges++;
	//gic_eoi(IRQ_ID_F2H0_0);
}




void stampa_f2h(void)
{
	double freq = 0.0;

	freq = g_edges/500.0;
	printf("\n\n\rFFT Pulse Ref. %ld kB - Pulse Tx Buff. %ld kB", coef_len/1024,pulse_len/1024);
	printf("\n\rFrequency F2H interrupt signal = %.2f kHz",freq);
	g_edges=0;
}
