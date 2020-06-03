OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(_start)
SECTIONS
{
    . = 0x40080000;

    .text   : ALIGN(4) { __start__ = ABSOLUTE(.); *(.crt0*) *(.text*); . = ALIGN(4); }
    .rodata : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data   : ALIGN(4) { *(.data*); . = ALIGN(8); __bss_start__ = ABSOLUTE(.); *(.bss* COMMON); . = ALIGN(8); __bss_end__ = ABSOLUTE(.);}

    . = ALIGN(4);
}
