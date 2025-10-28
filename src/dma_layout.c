#include <stdio.h>
#include "dma_layout.h"
#include "arm_mem_regions.h"
#include "arm_pio.h"
#include "schedule.h"
#include "f2h_interrupts.h"

extern volatile uint32_t *g_bank_coef_sel;
extern volatile uint32_t *g_bank_pulse_sel;
extern volatile uint32_t *g_arm_prf_counter;

volatile uint32_t g_channel   = 0;
volatile uint32_t g_coef_count = 1024;  // default: usa tutte
volatile uint32_t g_pulse_count = 1024;


static inline uint32_t mask_pow2(uint32_t n)
{
    // n ∈ {1,2,4,...,1024} -> restituisce n-1; se fuori, fallback 1024-1
    switch (n) {
    case 1u: case 2u: case 4u: case 8u: case 16u: case 32u: case 64u:
    case 128u: case 256u: case 512u: case 1024u: return n - 1u;
    default: return 1024u - 1u;
    }
}

void seq_config_set_channel(uint32_t channel, uint32_t coef_count_pow2, uint32_t pulse_count_pow2)
{
    if (channel > 4u) channel = 4u;

    g_channel = channel;

    g_coef_count   = (coef_count_pow2   == 0u) ? 1024u : coef_count_pow2;
    g_pulse_count = (pulse_count_pow2 == 0u) ? 1024u : pulse_count_pow2;

    arm_pio_write(g_arm_prf_counter,prf_setting(channel));

    // opzionale: potresti assertare che le size non superino la metà OCRAM
    // COEF OCRAM half = 512 KiB; PULSE OCRAM half = 64 KiB
    // (g_coef_pair_bytes[channel] <= 512KiB, g_pulse_pair_bytes[channel] <= 64KiB) -> già vero con le tabelle scelte
}

uint32_t pair_coef_source_addr(uint32_t idx)
{
    // cicla su g_coef_count (power of 2 → usa mask)
    uint32_t i = idx & mask_pow2(g_coef_count);
    uint32_t stride = g_coef_stride_bytes[g_channel];

    // Allineato per evitare crossing 4KiB e favorire burst lunghi
    return COEF_AREA_BASE + i * stride;
}

uint32_t pair_pulse_source_addr(uint32_t idx)
{
    uint32_t i = idx & mask_pow2(g_pulse_count);
    uint32_t stride = g_pulse_stride_bytes[g_channel];

    // PULSE parte dopo l’area COEF completamente allocata (in base al canale e a quante COEF opzioni usi)
    uint32_t coef_stride = g_coef_stride_bytes[g_channel];
    uint32_t base_pulse = PULSE_AREA_BASE(g_coef_count, coef_stride);

    return base_pulse + i * stride;
}

void coef_bank_sel(uint32_t bank)
{ // 0=A, 1=B
    arm_pio_write(g_bank_coef_sel, (bank & 0x1u));
}

void pulse_bank_sel(uint32_t bank)
{ // 0=A, 1=B
    arm_pio_write(g_bank_pulse_sel, (bank & 0x1u));
}

/*uint32_t coef_len_for_channel(uint32_t ch)
{
    switch (ch) {
        case 0: return  64*1024;   // 64 KiB
        case 1: return 128*1024;   // 128 KiB
        case 2: return 256*1024;   // 256 KiB
        case 3: return 512*1024;   // 512 KiB
        case 4: return 960*1024;   // 512 KiB + 256 KiB + 128 KiB + 64 KiB
        default: return 64*1024;
    }
}*/

uint32_t coef_len_for_channel(uint32_t ch)
{
    switch (ch) {
        case 0: return 4*1024;   // 64 KiB
        case 1: return 4*1024;   // 128 KiB
        case 2: return 4*1024;   // 256 KiB
        case 3: return 4*1024;   // 512 KiB
        default: return 4*1024;
    }
}

uint32_t pulse_len_for_channel(uint32_t ch)
{
    switch (ch) {
        case 0: return  1*1024;    // 1 KiB
        case 1: return 16*1024;    // 16 KiB
        case 2: return 32*1024;    // 32 KiB
        case 3: return 64*1024;    // 64 KiB
        default: return 1*1024;
    }
}

uint32_t prf_setting(uint32_t ch)
{
    switch (ch) {
        case 0: return  4000;   // 18.75 kHz
        case 1: return  7500;   // 10 kHz
        case 2: return 15000;   // 5 kHz
        case 3: return 30000;   // 2.5 kHz
       // case 4: return 60000;   // 1.25 kHz
        default: return 150000; // 500 Hz
    }
}

extern volatile uint32_t g_edges;

void change_pulse(void)
{
	static uint32_t channel = 0;
	// Esempi:
	 // channel = 0..3
	 // coef_count_pow2  = 1,2,4,...,1024
	 // pulse_count_pow2 = 1,2,4,...,1024

	if (channel > 3) channel = 0;
	seq_config_set_channel(channel, /*coef_count*/ 1024, /*pulse_count*/ 1024);
	// oppure: channel 1, solo 8 COEF e 16 PULSE
	// seq_config_set_channel(1, 8, 16);
	channel += 1;
	g_edges=0;
	sched_insert(CORE0,SCHED_ONETIME,stampa_f2h,500);
}

