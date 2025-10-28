ALT_DEVICE := ARRIA10
ALT_DEVICE_FAMILY := soc_a10

HWLIBS_SRC ?= alt_bridge_manager.c alt_clock_manager.c alt_ecc.c alt_fpga_manager.c alt_reset_manager.c alt_sdram.c alt_system_manager.c  alt_16550_uart.c alt_cache.c alt_dma.c alt_ethernet.c alt_generalpurpose_io.c alt_i2c.c alt_mmu.c alt_qspi.c alt_spi.c alt_watchdog.c  alt_address_space.c alt_can.c alt_dma_program.c alt_eth_phy_ksz9031.c alt_globaltmr.c alt_interrupt.c alt_nand.c alt_sdmmc.c alt_timers.c

UART_DEFINES ?= -DPRINTF_UART
HWLIBS_SRC += alt_printf.c alt_p2uart.c
HWLIBS_SRC += alt_base.c

# =======================
# Oggetti CORE0 / CORE1
# =======================
ALL_SRC_CORE0 = $(HWLIBS_SRC) $(SRC_FILE)

# Per CORE1 togli solo alt_base.c (alt_interrupt.c SERVE a Core1)
HWLIBS_SRC_CORE1 = $(filter-out alt_base.c, $(HWLIBS_SRC))
ALL_SRC_CORE1    = $(HWLIBS_SRC_CORE1) $(SRC_FILE_CORE1)

# -------- OBJ DIR separati --------
OBJ_DIR0 := objs_$(ALT_DEVICE)_core0
OBJ_DIR1 := objs_$(ALT_DEVICE)_core1

OBJ0 := $(patsubst %.c,$(OBJ_DIR0)/%.o,$(ALL_SRC_CORE0))
OBJ0 := $(patsubst %.S,$(OBJ_DIR0)/%.o,$(OBJ0))
OBJ0 := $(patsubst %.s,$(OBJ_DIR0)/%.o,$(OBJ0))

OBJ1 := $(patsubst %.c,$(OBJ_DIR1)/%.o,$(ALL_SRC_CORE1))
OBJ1 := $(patsubst %.S,$(OBJ_DIR1)/%.o,$(OBJ1))
OBJ1 := $(patsubst %.s,$(OBJ_DIR1)/%.o,$(OBJ1))

# -------- Percorsi include / toolchain --------
HWLIBS_SRC_ROOT = hwlib

INCLUDE_DIRS := -Ilib \
                -I$(HWLIBS_SRC_ROOT)/include \
                -I$(HWLIBS_SRC_ROOT)/include/$(ALT_DEVICE_FAMILY) \
                -I$(HWLIBS_SRC_ROOT)/include/$(ALT_DEVICE_FAMILY)/socal \
                -Iinc

ifneq ($(strip $(NEWLIB_ROOT)),)
INCLUDE_DIRS += -I$(NEWLIB_ROOT)/include
endif

# VPATH per risolvere i file
SRC_PATH = $(subst $() $(),:,$(dir $(ALL_SRC_CORE0))) , src , src_core1
VPATH = $(HWLIBS_SRC_ROOT)/src/hwmgr/:$(HWLIBS_SRC_ROOT)/src/hwmgr/$(ALT_DEVICE_FAMILY):$(HWLIBS_SRC_ROOT)/src/utils:$(SRC_PATH)

# -------- Toolchain --------
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)gcc
LDRAW := $(CROSS_COMPILE)ld
NM := $(CROSS_COMPILE)nm
OD := $(CROSS_COMPILE)objdump
OC := $(CROSS_COMPILE)objcopy

TOOLCHAIN_CC := $(shell command -v $(CC) 2>/dev/null)
ifeq ($(strip $(TOOLCHAIN_CC)),)
$(error Unable to locate compiler '$(CC)'. Please install an ARM EABI toolchain or override CROSS_COMPILE)
endif

# -------- Output names (ELF0/ELF1 definiti nel Makefile principale) --------
BIN0 := $(ELF0:.axf=.bin)
BIN1 := $(ELF1:.axf=.bin)

# u-boot image finale di core0
IMG := app_core0.uimg
DEFAULT_MKIMAGE := $(wildcard ../bootloader/u-boot-socfpga/tools/mkimage)
MKIMAGE ?= $(DEFAULT_MKIMAGE)
ifeq ($(strip $(MKIMAGE)),)
MKIMAGE := $(shell command -v mkimage 2>/dev/null)
endif
MKIMAGE := $(strip $(MKIMAGE))

# oggetto "binario" di core1 da linkare in core0
CORE1_BIN_OBJ := app_core1_bin.o

# -------- Linker scripts --------
LINKER_SCRIPT0 := linkerscripts/arria10-core0-ocr.ld
LINKER_SCRIPT1 := linkerscripts/arria10-core1-ddr.ld

# -------- Flags comuni --------
MULTILIBFLAGS := -mcpu=cortex-a9 -mfloat-abi=softfp -mfpu=neon
CFLAGS_COMMON := -g -O0 -Wall $(MULTILIBFLAGS) $(INCLUDE_DIRS) -D$(ALT_DEVICE_FAMILY) $(UART_DEFINES) -D$(ALT_DEVICE) -fdata-sections -ffunction-sections -ffreestanding -fno-pic -fno-pie
LDFLAGS_COMMON := $(MULTILIBFLAGS) --specs=nosys.specs -Wl,--gc-sections
ifneq ($(strip $(NEWLIB_ROOT)),)
LDFLAGS_COMMON += -B$(NEWLIB_ROOT)/lib --sysroot=$(NEWLIB_ROOT)/lib
endif

# Core1: baremetal puro (niente crt/newlib) + disabilita vector table HAL
CFLAGS_CORE1 := $(CFLAGS_COMMON) -DCORE1 -DALT_INT_PROVISION_VECTOR_TABLE=0
# Se la tua HWLIB richiede anche di non provisionare lo SP, scommenta:
CFLAGS_CORE1 += -DALT_INT_PROVISION_STACK_POINTER=0


LDFLAGS_CORE1 := -T$(LINKER_SCRIPT1) -nostartfiles -nostdlib $(MULTILIBFLAGS) -Wl,--gc-sections
LIBS_CORE1    := -lgcc

# -------- Varie --------
ODFLAGS := -DS
OCFLAGS := -O binary

ifeq ($(or $(COMSPEC),$(ComSpec)),)
  RM := rm -rf
else
  RM := cs-rm -rf
endif
CP := cp -f

# ===== Targets (NO ricorsione!) =====
.PHONY: all clean help copy_hwlib remove_hwlib

# Catena: uimg <- bin0 <- elf0 (linka core1_bin.o) <- core1_bin.o <- bin1 <- elf1
all: copy_hwlib $(OBJ_DIR0) $(OBJ_DIR1) $(IMG)

# ---- IMG (u-boot standalone) ----
ifeq ($(strip $(MKIMAGE)),)
$(IMG): $(BIN0)
	@echo "mkimage not found; creating placeholder $@" >&2
	@printf "mkimage not available in this environment\n" > $(IMG)
else
$(IMG): $(BIN0)
	$(MKIMAGE) -A arm -T standalone -C none \
	  -a 0xffe00040 -e 0xffe00080 \
	  -n "baremetal image" -d $(BIN0) $(IMG)
endif

# ---- BIN0 da ELF0 ----
$(BIN0): $(ELF0)
	$(OD) $(ODFLAGS) $(ELF0) > $(ELF0).objdump
	$(OC) $(OCFLAGS) $(ELF0) $(BIN0)

# ---- ELF0 linka anche l’oggetto binario di core1 ----
$(ELF0): $(OBJ0) $(CORE1_BIN_OBJ)
	$(LD) -T$(LINKER_SCRIPT0) $(LDFLAGS_COMMON) $(OBJ0) $(CORE1_BIN_OBJ) -o $@

# ---- oggetto binario (core1.bin → .o con simboli start/end/size) ----
$(CORE1_BIN_OBJ): $(BIN1)
	$(LDRAW) -r -b binary -o $(CORE1_BIN_OBJ) $(BIN1)

# ---- BIN1 da ELF1 ----
$(BIN1): $(ELF1)
	$(OD) $(ODFLAGS) $(ELF1) > $(ELF1).objdump
	$(OC) $(OCFLAGS) $(ELF1) $(BIN1)

# ---- ELF1 (baremetal) ----
$(ELF1): $(OBJ1)
	$(LD) $(LDFLAGS_CORE1) $(OBJ1) $(LIBS_CORE1) -o $@

# ---- Utility già presenti ----
copy_hwlib: hwlib.stamp
hwlib.stamp:
	cp $(HWLIBS_ROOT) . -rnL
	touch $@

remove_hwlib:
	rm -rf hwlib hwlib.stamp

# ---- Directory obj (order-only) ----
$(OBJ_DIR0) $(OBJ_DIR1):
	mkdir -p $@

# ----- Core0 compile rules -----
$(OBJ_DIR0)/%.o: %.c Makefile Makefile.inc | $(OBJ_DIR0)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

$(OBJ_DIR0)/%.o: %.S Makefile Makefile.inc | $(OBJ_DIR0)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

$(OBJ_DIR0)/%.o: %.s Makefile Makefile.inc | $(OBJ_DIR0)
	@mkdir -p $(dir $@)
	$(CC) -x assembler $(MULTILIBFLAGS) -c $< -o $@

# ----- Core1 compile rules -----
$(OBJ_DIR1)/%.o: %.c Makefile Makefile.inc | $(OBJ_DIR1)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_CORE1) -c $< -o $@

$(OBJ_DIR1)/%.o: %.S Makefile Makefile.inc | $(OBJ_DIR1)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_CORE1) -c $< -o $@

$(OBJ_DIR1)/%.o: %.s Makefile Makefile.inc | $(OBJ_DIR1)
	@mkdir -p $(dir $@)
	$(CC) -x assembler $(MULTILIBFLAGS) -DCORE1 -c $< -o $@

# ---- Clean ----
clean:
	$(RM) $(OBJ_DIR0) $(OBJ_DIR1) $(ELF0) $(ELF1) $(BIN0) $(BIN1) $(IMG) $(CORE1_BIN_OBJ) *.objdump *.map
