/* A ROM header with no modules and no files: the kernel runs from the .nds image, not from a romimage-built nk.bin. */
#include <windows.h>
#include <pehdr.h>
#include <romldr.h>

ROMHDR g_romhdr;
