#include <string.h>
#include "types.h"
#include "PXI.h"

#define KHC3DS_NO_ADDR_TRANSLATION
#include "../../takeover.h"

#define CFG11_SHAREDWRAM_32K_DATA(i)    (*(vu8 *)(0x10140000 + i))
#define CFG11_SHAREDWRAM_32K_CODE(i)    (*(vu8 *)(0x10140008 + i))
#define CFG11_DSP_CNT                   (*(vu8 *)0x10141230)

static void resetDsp(void)
{
    CFG11_DSP_CNT = 2; // PDN_DSP_CNT
    for(vu32 i = 0; i < 10; i++);

    CFG11_DSP_CNT = 3;
    for(vu32 i = 0; i < 10; i++);

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_DATA(i) = i << 2; // disabled, master = arm9

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_CODE(i) = i << 2; // disabled, master = arm9

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_DATA(i) = 0x80 | (i << 2); // enabled, master = arm9

    for(u32 i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_CODE(i) = 0x80 | (i << 2); // enabled, master = arm9
}

static void doFirmlaunch(void)
{
    // Fake firmlaunch

    while(PXIReceiveWord() != 0x44836);
    PXISendWord(0x964536);

    while(PXIReceiveWord() != 0x44837);
    PXIReceiveWord(); // High FIRM titleId
    PXIReceiveWord(); // Low FIRM titleId
    resetDsp();

    while(PXIReceiveWord() != 0x44846);

    *(vu32 *)0x1FFFFFFC = 0x22000000;
    
    ((void (*)(void))0x22100000)();
    __builtin_unreachable();
}

static void p9McShutdown(void)
{
  // Fake p9 shutdown

  while(PXIReceiveWord() != 0x10000);

  PXISendWord(0);
  PXISendWord(0x10040);
  PXISendWord(0);
}

// Must be position-indepedendent:

void p9TakeoverMain(BlobLayout *layout, u32 numCores, void (*callback)(void *p), void *p)
{
    khc3dsPrepareL2Table(layout);
    u32 l2TablePa = (u32)layout->l2table;

    ((vu32 *)0x1FFF8000)[KHC3DS_MAP_ADDR>>20] = l2TablePa | 1;
    ((vu32 *)0x1FFFC000)[KHC3DS_MAP_ADDR>>20] = l2TablePa | 1;

    if (numCores == 4) {
        ((vu32 *)0x1F3F8000)[KHC3DS_MAP_ADDR>>20] = l2TablePa | 1;
        ((vu32 *)0x1F3FC000)[KHC3DS_MAP_ADDR>>20] = l2TablePa | 1;
    }

    callback(p);
    p9McShutdown();
    doFirmlaunch();
}
