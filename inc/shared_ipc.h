#pragma once
#include <stdint.h>

#define SHM_MAGIC_BOOT   0xC0DE1DEAu
#define SHM_MAGIC_READY  0xC0DEBEEFu

typedef struct __attribute__((packed))
{
    volatile uint32_t magic;        // handshake Core0/Core1
    volatile uint32_t core0_ready;  // 1 quando Core0 ha finito init
    volatile uint32_t core1_ready;  // 1 quando Core1 è partito
    volatile uint32_t trig_count;   // contatore “esempio”
    volatile uint32_t core1_timer;   // contatore “esempio”
    volatile uint32_t log_head;     // per eventuale ring buffer log
    volatile uint32_t log_tail;
    volatile uint32_t reserved[58]; // padding a 256 byte (se vuoi allineare)
    // ... spazio a piacere (comandi, parametri, mailboxes, ecc.)
} shm_ctrl_t;

#define SHM_BASE 0x3F000000u
#define SHM_SIZE 0x01000000u


#define SHM_CTRL   ((shm_ctrl_t *)(uintptr_t)(SHM_BASE))
