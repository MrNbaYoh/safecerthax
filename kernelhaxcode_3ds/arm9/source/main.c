#include <string.h>
#include "types.h"
#include "PXI.h"
#include "fatfs/ff.h"

static FATFS sdFs;

TakeoverParameters g_takeoverParameters = {};

void arm9main(void)
{
    UINT rd;
    memcpy(&g_takeoverParameters, (const void *)0x22200000, sizeof(g_takeoverParameters));
    size_t fileOffset = g_takeoverParameters.payloadFileOffset;

    if(g_takeoverParameters.firmTid != 0xFFFFFFFF) {
      switch (g_takeoverParameters.firmTid & 0xFFFF) {
          case 0x0003:
          case 0x0002:
              PXISendWord(0x0000CAFE);
              break;
          default:
              break;
      }
    } else {
      PXISendWord(0x0000CAFE);
    }

    while (!PXIIsSendFIFOEmpty());

    // Read the payload and tell the Arm11 stub whether or not we could read it
    FRESULT fsRes = f_mount(&sdFs, "0:", 1);
    FIL f;
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0000 | fsRes);
        for (;;);
    }

    fsRes = f_open(&f, g_takeoverParameters.payloadFileName, FA_READ);
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0100 | fsRes);
        for (;;);
    }

    FSIZE_t fileSize = f_size(&f);
    if (fileSize < fileOffset || fileSize - fileOffset > 0xFFFE00) {
        PXISendWord(0xCACA0200 | 0);
        for (;;);
    }
    fsRes = f_lseek(&f, fileOffset);
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0200 | fsRes);
        for (;;);
    }

    fsRes = f_read(&f, (void *)0x23F00000, fileSize - fileOffset, &rd);
    if (fsRes != FR_OK) {
        PXISendWord(0xCACA0300 | fsRes);
        for (;;);
    }
    f_close(&f);
    f_mount(NULL, "0:", 1);

    PXISendWord(0);
    while (!PXIIsSendFIFOEmpty());

    while (PXIReceiveWord() != 0xC0DE);
    // Return and chainload.
}
