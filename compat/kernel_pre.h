#include <windows.h>
#include <pkfuncs.h>
#include <mkfuncs.h>
#include <psyscall.h>
#include <mwinbase.h>
#include <pwinreg.h>
#include <stdarg.h>
/* CE's va_start assumes MS stack layout; derive the same char* from GCC's AAPCS va_list instead. */
#undef va_start
#define va_start(ap, v) do { __builtin_va_list __nk_ca; __builtin_va_start(__nk_ca, v); (ap) = *(va_list *)&__nk_ca; } while (0)

/* DSi flat VM: VA == PA. The "secure section" (CE's single kernel VA section) is the 32 MB window holding the 16 MB of RAM. */
#undef SECURE_SECTION
#undef SECURE_VMBASE
#undef IsSecureVa
#define SECURE_SECTION      1
#define SECURE_VMBASE       (SECURE_SECTION << SECTION_SHIFT)
#define IsSecureVa(va)      (SECURE_SECTION == ((DWORD) (va) >> SECTION_SHIFT))
/* RAM that VirtualAlloc may hand out (committed 1:1); everything below FLAT_POOL_LO belongs to the kernel page pool. */
#define FLAT_POOL_LO        0x02600000   /* TEMP: was 0x02800000; +2 MB for VirtualAlloc (shell32+gx.dll ran out) */
#define FLAT_POOL_HI        0x02FE0000
