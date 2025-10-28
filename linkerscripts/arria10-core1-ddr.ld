/* Arria 10 - Core1
 * Immagine in DDR con entry fisica a 0x2000_0000
 */
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start_core1)

/* Alias richiesti da HWLIB (alt_base.c) */
PROVIDE(_stack_top = __stack_top__);
PROVIDE(_vectors   = __vectors_start__);

/* Mappa memoria */
MEMORY
{
  DDR_PRIV (rwx) : ORIGIN = 0x20000000, LENGTH = 0x1E000000   /* 0x2000_0000 .. 0x3EFF_FFFF */
  SHM      (rw ) : ORIGIN = 0x3F000000, LENGTH = 0x01000000   /* 16MiB condivisi */
}

SECTIONS
{
  /* Base immagine in DDR */
  . = ORIGIN(DDR_PRIV);
  __image_base__ = .;

  /* --- 1) RESET HANDLER all'inizio assoluto dell'immagine --- */
  .startup : ALIGN(4)
  {
    /* _start_core1 sta in .text.startup._start_core1 (vedi attributo nella tua C) */
    KEEP(*(.text.startup._start_core1))
  } > DDR_PRIV

  /* --- 2) Codice --- */
  .text :
  {
    . = ALIGN(4);
    __text_start__ = .;
    *(.text.startup*)           /* altro startup (oltre a _start_core1 già preso sopra) */
    *(.init)
    KEEP(*(.text.__core1_unwind_stub))
    *(.text*)
    *(.fini)
    __text_end__ = .;
  } > DDR_PRIV

  /* --- 3) Vettori in DDR (punterai VBAR a __vectors_start__) --- */
  .vectors :
  {
    . = ALIGN(0x100);
    __vectors_start__ = .;
    KEEP(*(.vectors))
    KEEP(*(.isr_vector))
    __vectors_end__ = .;
  } > DDR_PRIV

  /* --- 4) Unwind tables --- */
  .ARM.extab :
  {
    . = ALIGN(4);
    __extab_start = .;
    KEEP(*(.ARM.extab*))
    __extab_end = .;
  } > DDR_PRIV

  .ARM.exidx :
  {
    . = ALIGN(4);
    __exidx_start = .;
    KEEP(*(.ARM.exidx*))
    __exidx_end = .;
  } > DDR_PRIV

  /* --- 5) Dati/rodata --- */
  .rodata :
  {
    . = ALIGN(4);
    __rodata_start__ = .;
    *(.rodata*)
    __rodata_end__ = .;
  } > DDR_PRIV

  .data :
  {
    . = ALIGN(4);
    __data_start__ = .;
    *(.data*)
    __data_end__ = .;
  } > DDR_PRIV
  __data_load__ = LOADADDR(.data);

  /* --- 6) BSS --- */
  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    __bss_end__ = .;
  } > DDR_PRIV

  /* Fine area “caricata” */
  . = ALIGN(16);
  __end__ = .;
  PROVIDE(end = __end__);
  __image_size__ = __end__ - __image_base__;

  /* Heap/stack markers */
  __heap_start__ = .;
  __heap_end__   = ORIGIN(DDR_PRIV) + LENGTH(DDR_PRIV) - 0x00020000; /* ~128KiB per lo stack */

  __stack_top__   = ORIGIN(DDR_PRIV) + LENGTH(DDR_PRIV);
  __stack_limit__ = __heap_end__;

  /* --- 7) Sezione in SHM: no-load --- */
  .shm (NOLOAD) :
  {
    . = ALIGN(64);
    __shm_start__ = ORIGIN(SHM);
    *(.shm*)
    __shm_end__   = ORIGIN(SHM) + LENGTH(SHM);
  } > SHM

  /* Comodi */
  __ram_origin__ = ORIGIN(DDR_PRIV);
  __ram_end__    = ORIGIN(DDR_PRIV) + LENGTH(DDR_PRIV);

  /* --- 8) Controlli di sicurezza a link-time --- */
  ASSERT(DEFINED(_start_core1), "Manca il simbolo _start_core1 (sezione .text.startup._start_core1).")
  ASSERT(_start_core1 == ORIGIN(DDR_PRIV),
         "_start_core1 NON è alla base dell'immagine (atteso 0x01000000).")
}

