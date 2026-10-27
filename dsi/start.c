/* DSi boot shim: the BlocksDS crt0 brings the console up and calls main(); we then hand the ARM9 to the CE kernel. */
#include <windows.h>
#include <pehdr.h>
#include <romldr.h>

extern ROMHDR g_romhdr;
extern char __end__[];
extern void KernelStart(void);

/* OEMAddressTable for ARMInit (MDValidateRomChain): no MMU, so VA == PA. Sizes are in MB. The ROM chain's XIP range is 0..16 MB
 * (the kernel image lives at the bottom of the flat space), RAM is 0x02000000..0x03000000. Terminated by a zero size. */
typedef struct { DWORD dwVA, dwPA, dwSize; } OEMPTE;
OEMPTE DsiOEMAddressTable[] = {
    { 0x00000000, 0x00000000, 16 },
    { 0x02000000, 0x02000000, 16 },
    { 0, 0, 0 },
};

#define RAM_LIMIT FLAT_POOL_LO   /* kernel page pool ends here; [FLAT_POOL_LO, FLAT_POOL_HI) is VirtualAlloc memory mapped 1:1 */

int main(void)
{
    DWORD free_start = ((DWORD)__end__ + 0xFFF) & ~0xFFFu;

    g_romhdr.ulRAMStart = 0x02000000;
    g_romhdr.ulRAMFree = free_start;
    g_romhdr.ulRAMEnd = RAM_LIMIT;
    g_romhdr.usCPUType = 0x0946;     /* IMAGE_FILE_MACHINE_ARM is what romimage writes; the kernel only reports it */
    KernelStart();
    return 0;
}
