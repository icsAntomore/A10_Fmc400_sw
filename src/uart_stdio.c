#include <sys/unistd.h>
#include <sys/stat.h>
#include <errno.h>
#if !defined(CORE1)
#include <stdio.h>
#endif
#include "uart_stdio.h"

#include "alt_clock_manager.h"


static ALT_16550_HANDLE_t s_uart1;
static int s_crlf = 0;

/* Inizializza UART1 e prepara lo standard I/O su UART1 (baud tipico: 115200). */
ALT_STATUS_CODE uart_stdio_init_uart1(uint32_t baud);

/* Facoltativo: CRLF su newline (abilita se ti serve \r\n) */
void uart_stdio_set_crlf(int enable);


ALT_STATUS_CODE uart_stdio_init_uart1(uint32_t baud)
{
    ALT_STATUS_CODE st;
    uint32_t l4sp_hz_u32 = 0;

    /* clock L4_SP (bus dei peripheral, usato dalle UART) */
    st = alt_clk_freq_get(ALT_CLK_L4_SP, &l4sp_hz_u32);
    if (st != ALT_E_SUCCESS) return st;

    /* init: (device, location=NULL, clock_freq, handle) */
    st = alt_16550_init(ALT_16550_DEVICE_SOCFPGA_UART1, NULL,
                        (alt_freq_t)l4sp_hz_u32, &s_uart1);
    if (st != ALT_E_SUCCESS) return st;

    /* baud: nella tua hwlib è (handle, baud) */
    st = alt_16550_baudrate_set(&s_uart1, baud);
    if (st != ALT_E_SUCCESS) return st;

    /* opzionale: configura 8N1 se disponibile nella tua hwlib
       // st = alt_16550_line_config_set(&s_uart1,
       //        ALT_16550_PARITY_DISABLE, ALT_16550_DATA_BITS_8, ALT_16550_STOP_BITS_1);
       // if (st != ALT_E_SUCCESS) return st;
    */

    (void)alt_16550_fifo_enable(&s_uart1);  /* ok anche se no-op */

    st = alt_16550_enable(&s_uart1);
    if (st != ALT_E_SUCCESS) return st;

    /*
        * Niente buffering su stdout/stderr.  L'implementazione standard
        * richiede il supporto della libreria C (newlib) per setvbuf().
        * Quando si compila senza di essa il linker non trova i simboli
        * (_impure_ptr, setvbuf).  Rendiamo quindi la chiamata facoltativa
        * e la eseguiamo solo se la toolchain fornisce newlib.
        */
#if !defined(CORE1)
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif


    return ALT_E_SUCCESS;
}

static ALT_STATUS_CODE uart_tx_wait_ms(uint32_t budget_ms)
{
    uint32_t i=0;
    while (i < budget_ms) {
    	i++;
    }
    return ALT_E_SUCCESS;
}

void uart_stdio_set_crlf(int enable) { s_crlf = enable ? 1 : 0; }

static ALT_STATUS_CODE uart_stdio_write_raw(char ch)
{
    return alt_16550_fifo_write(&s_uart1, &ch, 1);
}

ALT_STATUS_CODE uart_stdio_write_char(char ch)
{
    if (s_crlf && ch == '\n') {
        ALT_STATUS_CODE st = uart_stdio_write_raw('\r');
        if (st != ALT_E_SUCCESS) return st;
    }
    return uart_stdio_write_raw(ch);
}

ALT_STATUS_CODE uart_stdio_write_string(const char *str)
{
    while (*str) {
        ALT_STATUS_CODE st = uart_stdio_write_char(*str++);
        if (st != ALT_E_SUCCESS) return st;
    }
    return ALT_E_SUCCESS;
}



ALT_STATUS_CODE uart_stdio_write_hex32(uint32_t value)
{
    for (int shift = 28; shift >= 0; shift -= 4) {
        uint32_t nibble = (value >> shift) & 0xFu;
        char ch = (char)(nibble < 10 ? ('0' + nibble) : ('A' + (nibble - 10)));
        ALT_STATUS_CODE st = uart_stdio_write_char(ch);
        if (st != ALT_E_SUCCESS) return st;
    }
    uart_tx_wait_ms(115200);
    return ALT_E_SUCCESS;
}

/* ---------------- newlib syscalls redirect ---------------- */

int _write(int fd, const void *buf, size_t cnt)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        const char *p = (const char *)buf;
        size_t left = cnt;

        while (left) {
            char ch = *p++;
            if (s_crlf && ch == '\n') {
                if (alt_16550_fifo_write(&s_uart1, "\r", 1) != ALT_E_SUCCESS) {
                    errno = EIO; return -1;
                }
            }
            if (alt_16550_fifo_write(&s_uart1, &ch, 1) != ALT_E_SUCCESS) {
                errno = EIO; return -1;
            }
            --left;
        }
        return (int)cnt;
    }
    errno = ENOSYS;
    return -1;
}

int _read(int fd, void *buf, size_t cnt)
{
    if (fd == STDIN_FILENO && cnt > 0) {
        char *p = (char *)buf;
        size_t got = 0;

        while (got < cnt) {
            if (alt_16550_fifo_read(&s_uart1, p, 1) == ALT_E_SUCCESS) {
                ++p; ++got;
            }
            /* polling: se non c'è dato, continua a girare */
        }
        return (int)got;
    }
    errno = ENOSYS;
    return -1;
}

/* minimi per newlib */
int _close(int fd) { (void)fd; return 0; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { return (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO); }
off_t _lseek(int fd, off_t pos, int whence) { (void)fd; (void)pos; (void)whence; return -1; }
