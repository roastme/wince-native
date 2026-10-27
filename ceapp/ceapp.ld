/* Layout of a GCC-built CE application before tools/mkexe.py turns it into a PE. Headers take the first page at 0x10000. */
ENTRY(WinMainCRTStartup)
SECTIONS
{
    . = 0x11000;
    .text : { *(.text .text.* .glue_7 .glue_7t .vfp11_veneer .v4_bx) *(.rodata .rodata.*) }
    . = ALIGN(0x1000);
    .data : { *(.data .data.*) *(.iat) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.ARM.attributes) *(.comment) *(.note*) }
}
