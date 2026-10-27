//
// CE 5 CRT FILE/FILEX, part 1. Included BEFORE windows.h (CRT rules only).
//
// windows.h pulls in the SDK stdlib.h (windef.h -> winnt.h -> ctype.h), which
// declares FILE as void. This runs first and makes FILE the named FILEX tag
// instead. The struct body is in compat/crt/filex.h, after windows.h, because
// it needs CRITICAL_SECTION and HANDLE.
//
#ifndef _CE5_CRT_FILEX_PRE_H
#define _CE5_CRT_FILEX_PRE_H

#define _FILE_DEFINED
#define _FILEX_DEFINED

struct _ce5_filex;
typedef struct _ce5_filex _FILEX;
typedef _FILEX FILEX;
typedef _FILEX FILE;

#endif
