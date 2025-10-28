#include "alt_bridge_manager.h"
#include "arm_pio.h"
#include "arm_mem_regions.h"
#include <stdint.h>
#include <stdint.h>
#include "socal/socal.h"

extern volatile uint32_t *g_arm_pio_data;

static inline ALT_STATUS_CODE arm_mm_require_pio_ready(void)
{
    return g_arm_pio_data ? ALT_E_SUCCESS : ALT_E_ERROR;
}

// Scrive un valore a 32 bit su tutte le linee del PIO (output-only).
ALT_STATUS_CODE arm_pio_write(volatile uint32_t *add_pio, uint32_t value)
{
	ALT_STATUS_CODE s = arm_mm_require_pio_ready();
	if (s != ALT_E_SUCCESS)
		return s;

	//*add_pio = value;
	alt_write_word((void *)add_pio, value);
	return ALT_E_SUCCESS;
}

// Legge il registro DATA (per PIO output riflette l'ultimo valore scritto).
ALT_STATUS_CODE arm_pio_read(volatile uint32_t *add_pio, uint32_t *out_value)
{
	if (out_value == 0)
		return ALT_E_BAD_ARG;

	ALT_STATUS_CODE s = arm_mm_require_pio_ready();
	if (s != ALT_E_SUCCESS)
		return s;

	//*out_value = *g_arm_pio_data;
	*out_value = alt_read_word(add_pio);
	return ALT_E_SUCCESS;
}

// Setta a 1 i bit indicati dalla mask, mantenendo gli altri.
ALT_STATUS_CODE arm_pio_set_bits(volatile uint32_t *add_pio,uint32_t mask)
{
	ALT_STATUS_CODE s = arm_mm_require_pio_ready();
	if (s != ALT_E_SUCCESS)
		return s;

	uint32_t value = alt_read_word(add_pio) | mask;
	//*g_arm_pio_data = (val_now | mask);
	alt_write_word((void*)add_pio, value);
	return ALT_E_SUCCESS;
}

// Porta a 0 i bit indicati dalla mask, mantenendo gli altri.
ALT_STATUS_CODE arm_pio_clear_bits(volatile uint32_t *add_pio,uint32_t mask)
{
	ALT_STATUS_CODE s = arm_mm_require_pio_ready();
	if (s != ALT_E_SUCCESS)
		return s;

	uint32_t value = alt_read_word(add_pio) & ~mask;
	//*g_arm_pio_data = *g_arm_pio_data & ~mask;
	alt_write_word((void*)add_pio, value);
	return ALT_E_SUCCESS;
}

// Scrive con mask: data_out = (old & ~mask) | (value & mask)
ALT_STATUS_CODE arm_pio_write_masked(volatile uint32_t *add_pio, uint32_t value, uint32_t mask)
{
	ALT_STATUS_CODE s = arm_mm_require_pio_ready();
	if (s != ALT_E_SUCCESS)
		return s;

	//uint32_t cur = *g_arm_pio_data;
	uint32_t cur = alt_read_word(add_pio);
	uint32_t value_final = (cur & ~mask) | (value & mask);
	//*g_arm_pio_data = (cur & ~mask) | (value & mask);
	alt_write_word((void*)add_pio, value_final);
	return ALT_E_SUCCESS;
}


void ledsys_core0(void)
{
    static int toggle = 0;

    if ((toggle & 0x01) ==  1)
    	arm_pio_set_bits(g_arm_pio_data,0x00000001);     // setta il bit 0
    else
    	arm_pio_clear_bits(g_arm_pio_data,0x00000001);   // azzera il bit 0

    toggle ^= 1;
}

void ledsys_core1(void)
{
    static int toggle = 0;

    if ((toggle & 0x01) ==  1)
    	arm_pio_set_bits(g_arm_pio_data,0x00000002);     // setta il bit 1
    else
    	arm_pio_clear_bits(g_arm_pio_data,0x00000002);   // azzera il bit 1

    toggle ^= 1;
}

