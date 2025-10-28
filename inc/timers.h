#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "alt_interrupt.h"
#include "alt_timers.h"

ALT_STATUS_CODE hps_timer_start(ALT_GPT_TIMER_t timer, uint32_t period_in_ms);
void hps_timer_stop(ALT_GPT_TIMER_t timer);
void core0_timer_int_callback(uint32_t icciar, void * context);
void core1_timer_int_callback(uint32_t icciar, void * context);
