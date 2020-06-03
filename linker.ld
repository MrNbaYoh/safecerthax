ENTRY(_start)
SECTIONS
{

    __heap_start__ = 0x0808C744;
    __heap_size__  = ABSOLUTE(0x1439C);
    __heap_end__   = __heap_start__ + __heap_size__;

    __start__ = 0x080932C0;
    __chunk_size__ = 0x2800;
    __fake_chunk__ = ABSOLUTE(__start__ + __chunk_size__);

    . = __start__;

    .text       : ALIGN(4) { *(.text.start) *(.text*); . = ALIGN(4); }
    .rodata     : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data       : ALIGN(4) { *(.data*); . = ALIGN(8); . = ALIGN(8); __bss_start = ABSOLUTE(.); *(.bss* COMMON); . = ALIGN(8); __bss_end = ABSOLUTE(.); }

    __padding__ = __fake_chunk__ - .;
    .fill   : { FILL(0x88888888); . = __padding__ - 1; BYTE(0x88); *(.fake_free_chunk*); }
}
