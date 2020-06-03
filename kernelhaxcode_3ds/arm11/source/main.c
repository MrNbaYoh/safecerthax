#include <string.h>
#include <stdarg.h>
#include "arm9.h"
#include "screen.h"
#include "draw.h"
#include "PXI.h"
#include "i2c.h"

#define HID_PAD                 (*(vu32 *)0x10146000 ^ 0xFFF)
#define FIRM_MAGIC              0x4D524946 // 'FIRM' in little-endian

static u32 posY = 10;
#define PRINT_FUNC(name, color, hang)\
void name(const char *fmt, ...)\
{\
    va_list args;\
    va_start(args, fmt);\
    posY = drawFormattedStringV(true, 10, posY, color, fmt, args);\
    va_end(args);\
    while (hang) { \
        while (HID_PAD == 0); \
        I2C_writeReg(I2C_DEV_MCU, 0x20, 1); /* shutdown */ \
    };\
}

PRINT_FUNC(print, COLOR_WHITE, false)
PRINT_FUNC(title, COLOR_TITLE, false)
PRINT_FUNC(success, COLOR_GREEN, false)
PRINT_FUNC(error, COLOR_RED, true)

extern const u32 prepareForFirmlaunchStub[];
extern u32 prepareForFirmlaunchStubSize;

TakeoverParameters g_takeoverParameters = {};

static inline const char *getFirmName(u64 tid)
{
    switch (tid & 0x0FFFFFFF) {
        case 2: return "NATIVE_FIRM";
        case 3: return "SAFE_FIRM";
        default: return "Arm9 exploit";
    }
}

static void launchFirm(u64 firmTid)
{
    Result res = p9McShutdown();
    if (res & 0x80000000) {
        error("Shutdown returned error 0x%08lx!\n", res);
    }

    res = firmlaunch(firmTid);
    if (res & 0x80000000) {
        error("Firmlaunch returned error 0x%08lx!\n", res);
    }
}

static void doFirmlaunchHax(u64 firmTid)
{
    static vu32 *const firmMagic = (vu32 *)0x24000000;
    static vu32 *const arm9Entrypoint = (vu32 *)0x2400000C; // old firmwares only

    launchFirm(firmTid);
    while(*firmMagic != FIRM_MAGIC); // Wait for the firm header to be written

    for (u32 i = 0; i < 0x10000; i++) {
        *arm9Entrypoint = 0x22100000;
    }

    while (PXIReceiveWord() != 0x0000CAFE);

    success("Got Arm9 arbitrary code execution!\n");
}

static void initFirm(void)
{
    vu32 *entrypointAddr = (vu32 *)0x1FFFFFFC;
    vu32 *lumaOperation = (vu32 *)0x1FF80004;

    // Wait for Process9 to write the new entrypoint
    // (launchFirm doesn't do that because of firmlaunchhax)
    u32 entrypoint;
    do {
        entrypoint = *entrypointAddr;
    }
    while (entrypoint == 0);

    bool isLuma = entrypoint == 0x1FF80000;
    *entrypointAddr = 0;

    if (isLuma) {
        // Handle Luma doing Luma things & wait for patching to finish -- note: can break in the future
        *lumaOperation = 7; // "ARM11_READY"
        while (*lumaOperation == 7);
        *lumaOperation = 7;

        while (*entrypointAddr == 0);
        entrypoint = *entrypointAddr;
    }

    if (isLuma) {
        print("Luma detected and bypassed.\n");
    }

    print("Arm11 entrypoint is 0x%08lx.\n", entrypoint);

    k9Sync();
    print("Synchronization with Kernel9 done.\n");

    p9InitComms();
    print("Synchronization with Process9 done.\n");
}

static void doSafeHax11(u64 firmTidMask)
{
    u64 firmTid = firmTidMask | 3;

    print("Launching %s...\n", getFirmName(firmTid));
    launchFirm(firmTid);
    initFirm();
    print("Doing firmlaunchhax...\n");
    doFirmlaunchHax(firmTid);
}

void arm11main(void)
{
    // Fill the screens with black while Luma (if present) may be loading sysmodules into VRAM.
    // prepareScreens will unset the regs.
    LCD_TOP_FILL_REG = LCD_FILL(0, 0, 0);
    LCD_BOTTOM_FILL_REG = LCD_FILL(0, 0, 0);

    memcpy(&g_takeoverParameters, (const void *)0x22200000, sizeof(g_takeoverParameters));

    u64 firmTid = g_takeoverParameters.firmTid;
    u64 firmTidMask = firmTid & ~0x0FFFFFFFull;

    // I2C_init(); <-- this fucks up
    prepareScreens();
    title("Post-firmlaunch Arm11 stub (%s)\n\n", getFirmName(firmTid));

    if (firmTid != 0xFFFFFFFF) {
        initFirm();
        switch (firmTid & 0x0FFFFFFF) {
            case 2:
                // If testing with Luma, patch Luma like this:
                // if (bootType != FIRMLAUNCH) ret += patchFirmlaunches(process9Offset, process9Size, process9MemAddr);
                print("Doing safehax v1.1...\n");
                doSafeHax11(firmTidMask);
                break;
            case 3:
                print("Doing firmlaunchhax...\n");
                doFirmlaunchHax(firmTid);
                break;
            default:
                error("FIRM TID not supported!\n");
                break;
        }
    } else {
        while (PXIReceiveWord() != 0x0000CAFE);
    }

    const char *fileName = g_takeoverParameters.payloadFileName;
    Result res = PXIReceiveWord();
    if ((res & 0x80000000) == 0) {
        success("%s read successfully!\n", fileName);
    } else {
        switch ((res >> 8) & 0xFF) {
            case 0:
                error("Failed to mount the SD card! (%u)\n", res & 0xFF);
                break;
            case 1:
                error("Failed to open %s! (%u)\n", fileName, res & 0xFF);
                break;
            case 2:
                error("%s is too big!\n", fileName);
                break;
            case 3:
                error("Failed to read %s! (%u)\n", fileName, res & 0xFF);
                break;
            default:
                error("Unknown filesystem error!\n");
                break;
        }
    }

    print("Preparing to chainload...\n");

    // Set framebuffer structure for Brahma payloads & reset framebuffer status.
    memcpy((void *)0x23FFFE00, defaultFbs, 2 * sizeof(struct fb));
    resetScreens();

    // Copy the firmlaunch stub.
    memcpy((void *)0x1FFFFC00, prepareForFirmlaunchStub, prepareForFirmlaunchStubSize);

    // We're ready.
    PXISendWord(0xC0DE);
    while (!PXIIsSendFIFOEmpty());

    ((void (*)(void))0x1FFFFC00)();
}
