/* Force-included when building Microsoft CE sources with GCC. */
#ifndef MS_COMPAT_H
#define MS_COMPAT_H
#define __cdecl
#define __stdcall
#define __fastcall
#define _cdecl
#define _stdcall
#define __int64 long long
#define __unaligned
#define __forceinline inline
#define _CRTIMP
#define _CRTAPI1
#define _CRTAPI2
#define __declspec(x) __ms_declspec_##x
#define __ms_declspec_dllimport
#define __ms_declspec_iw32
#define __ms_declspec_dllexport
#define __ms_declspec_noreturn __attribute__((noreturn))
#define __ms_declspec_naked __attribute__((naked))
#define __ms_declspec_nothrow
#define __ms_declspec_noinline __attribute__((noinline))
#define __ms_declspec_selectany __attribute__((weak))
#define __ms_declspec_align(x) __attribute__((aligned(x)))
#define __ms_declspec_novtable
#define __ms_declspec_intrin_type
#define __ms_declspec_restrict
#define __ms_declspec_noalias
#define __ms_declspec_deprecated

#define _inline inline
#define __inline__ inline
#define __RPC_FAR
#define interface struct
#define VERSION unsigned int
#define __export
#define __far
#define __near
#define _VARIANT_BOOL short
#define __pragma(x)
#define __CRTDECL
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 0
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT 0
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 0
/* SEH is not available: syntax-only stand-ins until the kernel gets its own fault recovery */
#define __try if (1)
#define __except(x) else if (0)
#define __finally if (1)
#define __leave
#define try
#define _declspec __declspec
#endif

#ifndef _T
#define _T(x) L##x
#endif
#define _alloca __builtin_alloca
