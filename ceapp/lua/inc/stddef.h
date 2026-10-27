/* The CE SDK has no stddef.h that the Lua sources can use. */
#ifndef _LUA_CE_STDDEF_H_
#define _LUA_CE_STDDEF_H_

typedef int ptrdiff_t;
typedef unsigned int size_t;
#ifndef NULL
#define NULL ((void *)0)
#endif
#define offsetof(t, m) ((size_t)&(((t *)0)->m))

#endif
