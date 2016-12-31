MEMORY {
   rom  (RX)  : ORIGIN = 0x0,        LENGTH = 512k
   ram1 (RWX) : ORIGIN = 0x10000000, LENGTH = 32k
   ram2 (RWX) : ORIGIN = 0x2007C000, LENGTH = 16k
}

_sram1  = ORIGIN(ram1);
_eram1  = _sram1 + LENGTH(ram1);
_sstack = _eram1;

SECTIONS {
   . = 0;

    _stext = .;
    .text : {
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata)
    } >rom

    .initcalls : {
        _sinitcalls = .;
        *(.initcall_early*)
        *(.initcall_0*)
        _einitcalls = .;
    } >rom

    .threads : {
        _sthreads = .;
        *(.threads*)
        _ethreads = .;
    }>rom

    _etext = .;

    .data : {
        _sdata = .;
        *(.data)
        _edata = .;
    } >ram1 AT >rom

    .bss : {
        _sbss = .;
        *(.bss)
        *(COMMON)
        _ebss = .;
    } >ram1 AT >rom
}

program_checksum = -(_sstack + _start + irq_nmi + irq_hardfault +
                     irq_memmanage + irq_busfault + irq_usagefault + 6);

ENTRY(_start)
