#include "asm_macros.s.h"

FUNCTION _start, .crt0
    b       start
    .word   __start__
    .word   arm9_bin

start:
    push    {r0-r4, lr}

    // Zero-fill bss
    ldr     r0, =__bss_start__
    mov     r1, #0
    ldr     r2, =__bss_end__
    sub     r2, r2, r0
    bl      memset

    pop     {r0-r3}
    bl      takeoverMain
    cmp     r0, #0
    popne   {r4, pc}

    // DSB, then Flush Prefetch Buffer (equivalent of ISB in later arch. versions). r0 = 0
    mcr     p15, 0, r0, c7, c10, 4
    mcr     p15, 0, r0, c7, c5, 4

    // Firmlaunch (kernPatchInterruptHandlerAndSvcPerms granted us access to this svc)
    mov     r0, #0
    ldr     r2, =g_takeoverParameters
    ldrd    r2, [r2]
    svc     0x7C

    b       .
END_FUNCTION

FUNCTION escalateAndPatch
    push {r0}

    // Check if Luma kext is there using GetSystemInfo. Always return 0 on vanilla
    ldr     r1, =0x20000
    mov     r2, #0
    svc     0x2A

    // If it's Luma (r0 = 1), use custom svc BetterBackdoor; otherwise use our injected SVC
    // Note: svc handler restores cpsr
    cmp     r0, #1
    ldreq   r0, =kernPatchInterruptHandlerAndSvcPerms
    popeq   {r1}
    svceq   0x80

    popne   {r0}
    svcne   0x30

    bx      lr
END_FUNCTION
