#include "kernel.h"
#include <string.h>

#include "PXI.h"
#include "arm9_bin.h"
#include "arm11_bin.h"

static KBaseInterruptEvent *originalKernelSetStateSgiEvent;

static void *kernKernelSetStageSgiHandlerHook(KBaseInterruptEvent *this, u32 irqId)
{
    // ASSUMPTION: KernelSetState was called with type 0 and no other call to it is being made while
    // we're running our exploit

    static volatile bool proceed = false;
    if (kernGetCurrentCoreId() == 0) {
        lcdDebug(false, 0, 255, 0);

        // Copy our sections:
        memmove((void *)KERNPA2VA(0x22000000), arm11_bin, arm11_bin_size);
        memmove((void *)KERNPA2VA(0x22100000), arm9_bin, arm9_bin_size);
        memmove((void *)KERNPA2VA(0x22200000), &g_takeoverParameters, sizeof(g_takeoverParameters));

        // DSB, Flush Prefetch Buffer (more or less "isb")
        // Needed to avoid race condition with Arm9 code
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");

        // Send Mode Control PXI (service id 0) command 1 (ShutdownPxi), telling p9 to stop doing stuff and to wait for command 0x44836
        // (this is what the pxi sysmodule normally does when terminating)
        PXISendWord(0);
        PXITriggerSync9IRQ();
        PXISendWord(0x10000);
        while (!PXIIsSendFIFOEmpty());

        // Wait for the reply, and ignore it.
        PXIReceiveWord();
        PXIReceiveWord();
        PXIReceiveWord();

        // Reset the PXI FIFOs like the pxi sysmodule does. This also drains potential P9 notifications, if any
        PXIReset();

        // CFG11_DSP_CNT must be zero when doing a firmlaunch
        CFG11_DSP_CNT = 0x00;

        // DSB
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
        proceed = true;
        // DSB
        __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
        __asm__ __volatile__ ("sev" ::: "memory");
    } else {
        do {
            __asm__ __volatile__ ("wfe" ::: "memory");
        } while (!proceed);
    }

    // Hooked
    return originalKernelSetStateSgiEvent->vtable->handleInterruptAndDispatch(originalKernelSetStateSgiEvent, irqId);
}

static KBaseInterruptEventVtable customKernelSetStateSgiEventVtable = {
    .handleInterruptAndDispatch = &kernKernelSetStageSgiHandlerHook,
};

static KBaseInterruptEvent customKernelSetStateSgiEvent = {
    .vtable = &customKernelSetStateSgiEventVtable,
};

void kernPatchInterruptHandlerAndSvcPerms(KInterruptData *interruptData)
{
    // Disable irq
    __asm__ __volatile__ ("cpsid i" ::: "memory");

    u32 numCores = IS_N3DS ? 4 : 2;
    originalKernelSetStateSgiEvent = interruptData[6].iEvent;

    for (u32 i = 0; i < numCores; i++) {
        interruptData[32 * i + 6].iEvent = &customKernelSetStateSgiEvent;
    }

    // Give access to svcKernelSetState
    register u32 sp asm("sp");
    u32 *threadSvcAcl = (u32 *)((sp & ~0xFFF) + 0xF38);
    threadSvcAcl[0x7C / 32] |= BIT(0x7C % 32);
}
