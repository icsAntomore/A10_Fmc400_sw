#include <inttypes.h>

#define IRQ_ID_F2H0_0 51
#define IRQ_ID_F2H0_1 52
#define IRQ_ID_F2H0_2 53
#define IRQ_ID_F2H0_3 54
#define IRQ_ID_F2H0_4 55
#define IRQ_ID_F2H0_5 56

void fpga_f2h0_isr(uint32_t icciar, void *ctx);
void sgdma0_int_callback(uint32_t icciar, void *ctx);
void sgdma1_int_callback(uint32_t icciar, void *ctx);
void sgdma2_int_callback(uint32_t icciar, void *ctx);
void sgdma3_int_callback(uint32_t icciar, void *ctx);
void sgdma4_int_callback(uint32_t icciar, void *ctx);
void stampa_f2h(void);
void stampa_trigger(void);
