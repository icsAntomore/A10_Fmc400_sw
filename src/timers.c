#include "alt_clock_manager.h"
#include "alt_fpga_manager.h"
#include "timers.h"
#include "shared_ipc.h"

unsigned long core0_ticks = 0;
unsigned long core1_ticks = 0;

ALT_STATUS_CODE hps_timer_start(ALT_GPT_TIMER_t timer, uint32_t period_in_ms)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    uint32_t freq;

    /* Determine the frequency of the timer */
    if (status == ALT_E_SUCCESS) {
        status = alt_clk_freq_get(ALT_CLK_MPU_PERIPH, &freq);
        //printf("INFO: Frequency = %" PRIu32 ".\n", freq);
    }

    /* Set the counter of the timer, which determines how frequently it fires. */
    if (status == ALT_E_SUCCESS) {
        uint32_t counter = (freq / 1000) * period_in_ms;

        status = alt_gpt_counter_set(timer, counter);
    }

    /* Set to periodic, meaning it fires repeatedly. */
    if (status == ALT_E_SUCCESS) {
        status = alt_gpt_mode_set(timer, ALT_GPT_RESTART_MODE_PERIODIC);
    }

    /* Set the prescaler of the timer to 0. */
    if (status == ALT_E_SUCCESS) {
        status = alt_gpt_prescaler_set(timer, 0);
    }

    /* Clear pending interrupts */
    if (status == ALT_E_SUCCESS) {
        if (alt_gpt_int_if_pending_clear(timer) == ALT_E_BAD_ARG) {
            status = ALT_E_BAD_ARG;
        }
    }

    /* Enable timer interrupts */
    if (status == ALT_E_SUCCESS) {
        status = alt_gpt_int_enable(timer);
    }

    /* Start the timer. */
    if (status == ALT_E_SUCCESS) {
        status = alt_gpt_tmr_start(timer);
    }

    return status;
}

void hps_timer_stop(ALT_GPT_TIMER_t timer)
{
    alt_gpt_tmr_stop(timer);
    alt_gpt_int_disable(timer);
}

void core0_timer_int_callback(uint32_t icciar, void * context)
{
	(void)alt_gpt_int_if_pending_clear(ALT_GPT_CPU_PRIVATE_TMR);
	core0_ticks++;
}

void core1_timer_int_callback(uint32_t icciar, void * context)
{
	(void)alt_gpt_int_if_pending_clear(ALT_GPT_CPU_PRIVATE_TMR);
	core1_ticks++;
}


