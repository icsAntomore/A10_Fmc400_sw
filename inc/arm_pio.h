/*#define ARM_PIO_OFST 0x00008060u // offset della periferica nello spazio FPGA
#define PIO_DATA_OFST 0x00u // registro DATA dei PIO semplici

// Indirizzo CPU del registro DATA della periferica
#define ARM_PIO_DATA_ADDR (LW_BRIDGE_BASE + ARM_PIO_OFST + PIO_DATA_OFST)*/

ALT_STATUS_CODE arm_pio_write(volatile uint32_t *add_pio, uint32_t value);
ALT_STATUS_CODE arm_pio_read(volatile uint32_t *add_pio, uint32_t *out_value);
ALT_STATUS_CODE arm_pio_set_bits(volatile uint32_t *add_pio, uint32_t mask);
ALT_STATUS_CODE arm_pio_clear_bits(volatile uint32_t *add_pio, uint32_t mask);
ALT_STATUS_CODE arm_pio_write_masked(volatile uint32_t *add_pio, uint32_t value, uint32_t mask);

void ledsys_core0(void);
void ledsys_core1(void);
