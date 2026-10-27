/* Real Windows Mobile modules, normalised by tools/wm6/mkpe.py (local only: Microsoft binaries are never redistributed). */
    .section .rodata.wm6, "a"
    .balign 4096
    .global wm6_coredll
wm6_coredll:
    .incbin WM6_COREDLL

    .balign 4096
    .global wm6_filesys
wm6_filesys:
    .incbin WM6_FILESYS
