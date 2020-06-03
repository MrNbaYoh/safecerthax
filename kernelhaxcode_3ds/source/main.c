#include <string.h>
#include "hooks.h"
#include "kernel.h"

#define MAKE_BRANCH(src,dst)    (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

void escalateAndPatch(u32 interruptDataAddr);

TakeoverParameters g_takeoverParameters = {};

static u32 *const axiwramStart = (u32 *)MAP_ADDR;
static u32 *const axiwramEnd = (u32 *)(MAP_ADDR + 0x80000);

static inline void *fixAddr(u32 addr)
{
    return (u8 *)MAP_ADDR + (addr - 0x1FF80000);
}

void panik(u32 x, ...);

static Result modifySvcTableAndInjectInterruptHandler(void)
{
    // Locate svc handler first: 00 6F 4D E9 STMFD           SP, {R8-R11,SP,LR}^ (2nd instruction)
    u32 *svcTableMirrorVa;
    for (svcTableMirrorVa = axiwramStart; svcTableMirrorVa < axiwramEnd && *svcTableMirrorVa != 0xE94D6F00; svcTableMirrorVa++);

    if (svcTableMirrorVa >= axiwramEnd) {
        lcdDebug(true, 255, 0, 0);
        return 0xDEAD2101;
    }

    // Locate the table
    while (*svcTableMirrorVa != 0) svcTableMirrorVa++;

    // Everything has access to SendSyncRequest1/2/3/4 (id 0x2E to 0x31)
    svcTableMirrorVa[0x30] = (u32)kernPatchInterruptHandlerAndSvcPerms;

    // Locate interrupt manager; it's first fields correspond to the array of SGI handlers per core
    // Then we replace the KernelSetState SGI (6) handler
    u32 *pos;
    for (pos = axiwramStart; pos < axiwramEnd && *pos != 0xD8A007FC; pos++);
    if (pos >= axiwramEnd) {
        lcdDebug(true, 255, 0, 0);
        return 0xDEAD2102;
    }

    // At this point we need to escalate because kernel bss isn't linearly mapped like kernel .text is
    escalateAndPatch(pos[1]);
    return 0;
}

static Result installFirmlaunchHook(void)
{
    // Find 14 FF 2F E1                 BX              R4      ; __core0_stub
    // This should be the first result
    u32 *hookLoc;
    for (hookLoc = axiwramStart; hookLoc < axiwramEnd && *hookLoc != 0xE12FFF14; hookLoc++);
    if (hookLoc >= axiwramEnd) {
        lcdDebug(true, 255, 0, 0);
        return 0xDEAD2002;
    }

    u8 *branchDst = (u8 *)fixAddr(0x1FFF4F00); // should be OK, let's check
    if (*(u32 *)fixAddr(0x1FFF4F00) != 0xFFFFFFFF) {
        lcdDebug(true, 255, 0, 0);
        return 0xDEAD2003;
    }

    memcpy(branchDst, kernelFirmlaunchHook, kernelFirmlaunchHookSize);

    *hookLoc = MAKE_BRANCH(hookLoc, branchDst);
    return 0;
}

Result takeoverMain(u64 firmTid, const char *payloadFileName, size_t payloadFileOffset)
{
    Result res = 0;
#ifdef KHC3DS_FORCE_FIRM_TID
    (void)firmTid;
    firmTid = KHC3DS_FORCE_FIRM_TID;
#endif
#ifdef KHC3DS_FORCE_FILE_NAME
    (void)payloadFileName;
    payloadFileName = KHC3DS_FORCE_FILE_NAME;
#endif
#ifdef KHC3DS_FORCE_FILE_OFFSET
    (void)payloadFileOffset;
    payloadFileOffset = KHC3DS_FORCE_FILE_OFFSET;
#endif

    g_takeoverParameters.firmTid = firmTid;
    g_takeoverParameters.kernelVersionMajor = KERNEL_VERSION_MAJOR;
    g_takeoverParameters.kernelVersionMinor = KERNEL_VERSION_MINOR;
    g_takeoverParameters.isN3ds = IS_N3DS;
    g_takeoverParameters.payloadFileOffset = payloadFileOffset;
    strncpy(g_takeoverParameters.payloadFileName, payloadFileName, 255);

    lcdDebug(true, 0, 255, 255);
    TRY(installFirmlaunchHook());
    lcdDebug(true, 255, 0, 255);
    TRY(modifySvcTableAndInjectInterruptHandler()); // and patches the thread's svc acl
    lcdDebug(true, 0, 255, 0);

    return res;
}
