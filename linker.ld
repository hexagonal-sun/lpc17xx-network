MEMORY {
   ram  (RWX)  : ORIGIN = 0xff0C0000, LENGTH = 15M
}

_sram  = ORIGIN(ram);
_eram  = _sram + LENGTH(ram);

SECTIONS {
    .text : {
        _stext = .;
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
