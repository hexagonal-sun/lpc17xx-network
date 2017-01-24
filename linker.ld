MEMORY {
   ram  (RWX)  : ORIGIN = 0x82000000, LENGTH = 2048M
}

_sram  = ORIGIN(ram);
_eram  = _sram + LENGTH(ram);

SECTIONS {
    _stext = .;
    .text : {
        *(.start_text*)
        *(.text*)
        *(.rodata)
    } >ram

    .initcalls : {
        _sinitcalls = .;
        *(.initcall_early*)
        *(.initcall_0*)
        _einitcalls = .;
    } >ram

    .threads : {
        _sthreads = .;
        *(.threads*)
        _ethreads = .;
    }>ram

    _etext = .;

    .data : {
        _sdata = .;
        *(.data)
        _edata = .;
    } >ram

    .bss : {
        _sbss = .;
        *(.bss)
        *(COMMON)
        _ebss = .;
    } >ram

    . += 32k;
    _sstack = ALIGN(8);
}

ENTRY(_start)
