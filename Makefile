#

#MEMORY ?= ddr
MEMORY ?= ocr


#HWLIBS_ROOT := ../../../tool_chain/armv7a/hwlib/
#NEWLIB_ROOT := ../../../tool_chain/gcc-arm-none-eabi/bin/

HWLIBS_ROOT ?= ../../../tool_chain/armv7a/hwlib/
NEWLIB_ROOT ?=

ADD_CFLAGS_GNU :=
ADD_CFLAGS_ARMCC :=

CORE1_SRC_DIR := core1

# =======================
# Sorgenti CORE0 
# =======================
SRC_FILE := main.c 
SRC_FILE += interrupts.c
SRC_FILE += timers.c 
SRC_FILE += arm_pio.c
SRC_FILE += schedule.c
SRC_FILE += f2h_interrupts.c
SRC_FILE += uart_stdio.c
SRC_FILE += arm_mem_regions.c
SRC_FILE += msgdma.c
SRC_FILE += dma_layout.c
SRC_FILE += qspi_utils.c


# =======================
# Sorgenti CORE1
# =======================
SRC_FILE_CORE1 := $(CORE1_SRC_DIR)/core1_main.c
SRC_FILE_CORE1 += $(CORE1_SRC_DIR)/core1_startup.c
SRC_FILE_CORE1 += $(CORE1_SRC_DIR)/libc_min.c
SRC_FILE_CORE1 += $(CORE1_SRC_DIR)/core1_exidx_stub.S
SRC_FILE_CORE1 += $(CORE1_SRC_DIR)/core1_vectors.S
#SRC_FILE_CORE1 += $(CORE1_SRC_DIR)/secondary_boot.c
SRC_FILE_CORE1 += arm_mem_regions.c
SRC_FILE_CORE1 += uart_stdio.c
SRC_FILE_CORE1 += interrupts.c
SRC_FILE_CORE1 += timers.c
SRC_FILE_CORE1 += arm_pio.c
SRC_FILE_CORE1 += schedule.c

ELF0 := app_core0.axf
ELF1 := app_core1.axf

# These parameters can be overriden
# LINKER_SCRIPT
# HWLIBS_SRC

include Makefile.inc
