.arm
.cpu        mpcore

.section    .data.hooks, "aw", %progbits

.type       kernelFirmlaunchHook, %function
.global     kernelFirmlaunchHook

kernelFirmlaunchHook:
    // Copy hook to 0x1FFFFC00 which is the normal location, to avoid getting overwritten
    ldr     r4, =0x1FFFFC00
    mov     r0, r4
    adr     r1, _kernelFirmlaunchHookStart
    adr     r2, kernelFirmlaunchHookEnd
    sub     r2, r1
    bl      _memcpy32
    bx      r4
.pool

_kernelFirmlaunchHookStart:
    ldr     r2, =0x1FFFFFFC
    mov     r1, #0
    str     r1, [r2]

    ldr     r0, =0x10163008 // PXI_SEND
    ldr     r1, =0x44846
    str     r1, [r0]        // Tell P9 we're ready

    // Wait for P9 to finish its job & chainload
    _waitForEpLoop:
        ldr     r0, [r2]
        cmp     r0, #0
        beq     _waitForEpLoop

    // Jump
    ldr     pc, =0x22000000

_memcpy32:
    add     r2, r0, r2
    _memcpy32_loop:
        ldr     r3, [r1], #4
        str     r3, [r0], #4
        cmp     r0, r2
        blo     _memcpy32_loop
    bx      lr
.pool
kernelFirmlaunchHookEnd:

.global     kernelFirmlaunchHookSize
kernelFirmlaunchHookSize:
    .word kernelFirmlaunchHookEnd - kernelFirmlaunchHook
