#pragma once
#include <stdint.h>



// === Tabelle dimensioni per CHANNEL ===
// COEF (coppia Re+Im) in byte
static const uint32_t g_coef_pair_bytes[4]   = {  64u*1024u, 64u*1024u, 256u*1024u, 512u*1024u };
// Stride DDR consigliato (≥ size, ben allineato) per ridurre latenze
static const uint32_t g_coef_stride_bytes[4] = { 128u*1024u, 128u*1024u, 512u*1024u, 1024u*1024u };

// PULSE (coppia Re+Im) in byte
static const uint32_t g_pulse_pair_bytes[4]   = {  1u*1024u, 16u*1024u,  32u*1024u,  64u*1024u };
static const uint32_t g_pulse_stride_bytes[4] = { 16u*1024u, 32u*1024u,  64u*1024u, 128u*1024u };

// Stato runtime (se vuoi tenerlo qui; altrimenti in un .c con accessor)
extern volatile uint32_t g_channel;      // 0..3
extern volatile uint32_t g_coef_count;    // quante COEF opzioni cicli (1..1024, potenze di 2)
extern volatile uint32_t g_pulse_count;  // quante PULSE opzioni cicli (1..1024, potenze di 2)

// Indici correnti (li hai già: coeff_pair_idx / pulse_pair_idx)
uint32_t pair_coef_source_addr(uint32_t idx);
uint32_t pair_pulse_source_addr(uint32_t idx);

// Offset banco OCRAM per la coppia corrente, in base al BANK toggle (0=A,1=B)
static inline uint32_t coef_bank_dst_off(uint32_t bank, uint32_t channel)
{
    // ping-pong: banco B parte dopo la metà usata da A (pari alla size corrente)
    return (bank == 0) ? g_coef_pair_bytes[channel] : 0u;
}
static inline uint32_t pulse_bank_dst_off(uint32_t bank, uint32_t channel)
{
    return (bank == 0) ? g_pulse_pair_bytes[channel] : 0u;
}

void coef_bank_sel(uint32_t bank);
void pulse_bank_sel(uint32_t bank);
uint32_t coef_len_for_channel(uint32_t ch);
uint32_t pulse_len_for_channel(uint32_t ch);

// Config veloce a runtime
void seq_config_set_channel(uint32_t channel, uint32_t coef_count_pow2, uint32_t pulse_count_pow2);
uint32_t prf_setting(uint32_t ch);
void change_pulse(void);
