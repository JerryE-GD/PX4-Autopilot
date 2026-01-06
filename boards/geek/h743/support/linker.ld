MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 16K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS
{
    .text : { KEEP(*(.isr_vector)) *(.text*) *(.rodata*) } > FLASH
    .data : AT(ORIGIN(FLASH)+LENGTH(FLASH)-SIZEOF(.data)) { *(.data*) } > RAM
    .bss : { *(.bss*) } > RAM
}
