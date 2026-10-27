//
// CE 5 CRT malloc.h stand-in. The SDK copy in OAK/SDK INC is the CE 6 shell
// header (CShellMalloc, ceshpriv.h) and cannot be used by the CRT. This file
// declares the CRT allocator entry points the CRT sources call.
//
#ifndef _CE5_CRT_MALLOC_H
#define _CE5_CRT_MALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t __cdecl _msize(void *memblock);
void * __cdecl _expand(void *memblock, size_t size);
int __cdecl _heapmin(void);

typedef int (__cdecl *_PNH)(size_t);
_PNH __cdecl _set_new_handler(_PNH pnh);
int __cdecl _set_new_mode(int newmode);
int __cdecl _query_new_mode(void);

#ifdef __cplusplus
}
#endif

#endif
