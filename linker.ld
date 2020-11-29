ENTRY(_start)
SECTIONS
{

    __heap_start__ = 0x0808C744;
    __heap_size__  = ABSOLUTE(0x1439C);
    __heap_end__   = __heap_start__ + __heap_size__;

    __chunk_size__ = 0x2800;
    __fake_free_chunk_offset__ = __chunk_size__ - .;

    .text       : ALIGN(4) { *(.text.start) *(.text*); . = ALIGN(4); }
    .rodata     : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data       : ALIGN(4) { *(.data*); . = ALIGN(8); . = ALIGN(8); __bss_start = .; *(.bss* COMMON); . = ALIGN(8); __bss_end = .; }
    .got        : ALIGN(4) { *(.got*); . = ALIGN(4); }
    .got.plt    : ALIGN(4) { *(.got.plt*); . = ALIGN(4); }

    __padding__ = __fake_free_chunk_offset__ - .;
    .fill   : { FILL(0x88888888); . = __padding__ - 1; BYTE(0x88); *(.fake_free_chunk*); }
}
