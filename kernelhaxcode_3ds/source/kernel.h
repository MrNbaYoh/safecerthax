#pragma once

#include "types.h"

struct KBaseInterruptEventVtable;

typedef struct KBaseInterruptEvent {
    struct KBaseInterruptEventVtable *vtable;
} KBaseInterruptEvent;

typedef struct KBaseInterruptEventVtable {
    void *(*handleInterruptAndDispatch)(KBaseInterruptEvent *this, u32 irqId);
} KBaseInterruptEventVtable;

typedef struct KInterruptData
{
    KBaseInterruptEvent *iEvent;
    bool disableUponReceipt;
    bool isDisabled;
    u8 priority;
    u8 padding;
} KInterruptData;

static_assert(sizeof(KInterruptData) == 8, "wrong interrupt data size");

static inline u32 kernGetCurrentCoreId(void)
{
    u32 coreId;
    __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5" : "=r"(coreId));
    return coreId & 3;
}

void kernPatchInterruptHandlerAndSvcPerms(KInterruptData *interruptData);
