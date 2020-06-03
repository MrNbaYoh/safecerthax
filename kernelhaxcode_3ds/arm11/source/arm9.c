#include <string.h>
#include <stdarg.h>
#include "fmt.h"
#include "PXI.h"

#define CFG11_DSP_CNT           (*(vu8 *)0x10141230)

extern TakeoverParameters g_takeoverParameters;
void print(const char *fmt, ...);

static struct {
    u8 syncStatus;
    u8 unitInfo;
    u8 bootEnv;
    u32 unused;
    u32 brahmaArm11Entrypoint;
    u32 arm11Entrypoint;
} volatile *const mailbox = (void *)0x1FFFFFF0;

u8 g_unitInfo;
u32 g_bootEnv;

static inline u32 getCmdbufSizeWords(u32 cmdhdr)
{
    return (cmdhdr & 0x3F) + ((cmdhdr & 0xFC0) >> 6) + 1;
}

// Called by k11's entrypoint function
void k9Sync(void)
{
    /*
        So... recent k9 implements _two_ synchronization mechanism, the legacy one and the new one. We'll
        use the legacy one. Newer one is below:

        mailbox->syncStatus = 1;
        while (mailbox->syncStatus != 2);
        mailbox->syncStatus = 3;
     */

    mailbox->syncStatus = 1;
    while (mailbox->syncStatus != 2);
    mailbox->syncStatus = 1;
    while (mailbox->syncStatus != 2);

    g_unitInfo = mailbox->unitInfo;
    g_bootEnv = mailbox->bootEnv;

    if (g_unitInfo == 3) {
      for (u32 i = 0; i < 0x800000; i++);
      mailbox->syncStatus = 1;
      while (mailbox->syncStatus != 2);
      mailbox->syncStatus = 3;
    }
}

// Code from 3ds_pxi
// Basically resets and syncs the PXI FIFOs with Process9
void p9InitComms(void)
{
    PXIReset();

    // Ensure that both the Arm11 and Arm9 send FIFOs are full, then agree on the shared byte.
    // Then flush, then agree a last time.
    do {
        while (!PXIIsSendFIFOFull()) {
            PXISendWord(0);
        }
    } while (PXIIsReceiveFIFOEmpty() || !PXIIsSendFIFOFull());

    PXISendByte(1);
    while (PXIReceiveByte() < 1);

    while (!PXIIsReceiveFIFOEmpty()) {
        PXIReceiveWord();
    }

    PXISendByte(2);
    while(PXIReceiveByte() < 2);
}

static void p9SendCmdbufWithServiceId(u32 serviceId, u32 *cmdbuf)
{
    // https://github.com/TuxSH/3ds_pxi/blob/master/source/sender.c#L40
    PXISendWord(serviceId);
    PXITriggerSync9IRQ(); //notify arm9
    PXISendBuffer(cmdbuf, getCmdbufSizeWords(cmdbuf[0]));
    while (!PXIIsSendFIFOEmpty());
}

static Result p9ReceiveCmdbuf(u32 *cmdbuf)
{
    cmdbuf[0] = PXIReceiveWord();
    u32 cmdbufSizeWords = getCmdbufSizeWords(cmdbuf[0]);
    if (cmdbufSizeWords > 0x40) {
        return 0xDEAD3001;
    }
    PXIReceiveBuffer(cmdbuf + 1, cmdbufSizeWords - 1);
    while (!PXIIsReceiveFIFOEmpty());

    return 0;
}

Result p9ReceiveNotification11(u32 *outNotificationId, u32 serviceId)
{
    u32 cmdbuf[0x40];
    Result res = 0;
    TRY(p9ReceiveCmdbuf(cmdbuf));

    switch (cmdbuf[0] >> 16) {
        case 1:
            *outNotificationId = cmdbuf[1];
            cmdbuf[0] = 0x10040;
            res = 0;
            break;
        default:
            *outNotificationId = 0;
            cmdbuf[0] = 0x40;
            res = 0xD900182F;
            break;
    }

    cmdbuf[1] = res;

    p9SendCmdbufWithServiceId(serviceId, cmdbuf);

    return res;
}

static Result p9SendSyncRequest(u32 serviceId, u32 *cmdbuf)
{
    u32 replyServiceId;
    Result res = 0;

    p9SendCmdbufWithServiceId(serviceId, cmdbuf);

    for (;;) {
        // Receive the request or reply cmdbuf from p9 (might be incoming notification)
        // We assume that if we didn't receive serviceId, we received a notification (this is not foolproof)
        // Pre-2.0 (or 3.0? not sure), p9 only has 5 services instead of 8 (no duplicate sessions for FS)
        while (PXIIsReceiveFIFOEmpty());
        replyServiceId = PXIReceiveWord();
        if (replyServiceId == serviceId) {
            TRY(p9ReceiveCmdbuf(cmdbuf));
            return getCmdbufSizeWords(cmdbuf[0]) == 0 ? 0 : cmdbuf[1];
        } else {
            u32 notificationId;
            TRY(p9ReceiveNotification11(&notificationId, replyServiceId));
            print("Received notification ID 0x%lx\n", notificationId);
        }
    }

    return res;
}

Result p9McShutdown(void)
{
    u32 cmdbuf[0x40];
    cmdbuf[0] = 0x10000;

    return p9SendSyncRequest(0, cmdbuf);
}

Result firmlaunch(u64 firmlaunchTid)
{
    PXIReset();

    // CFG11_DSP_CNT must be zero when doing a firmlaunch
    CFG11_DSP_CNT = 0x00;

    PXISendWord(0x44836);

    if (PXIReceiveWord() != 0x964536) {
        return 0xDEAD4001;
    }

    PXISendWord(0x44837);
    while (!PXIIsSendFIFOEmpty());

    PXISendWord((u32)(firmlaunchTid >> 32));
    PXISendWord((u32)firmlaunchTid);
    while (!PXIIsSendFIFOEmpty());

    mailbox->arm11Entrypoint = 0;
    PXISendWord(0x44846);
    while (!PXIIsSendFIFOEmpty());

    return 0;
}
