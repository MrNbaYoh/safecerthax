.arm

.section    .text.start, "ax", %progbits
.align      2
.global     _start
.type       _start, %function
_start:
    b       _start1
    b       _start2
    nop
    nop

_p9TakeoverParams:
    .word   0xCCCCCCCC
    .word   4
    .word   0xDDDDDDDD
    .word   0xEEEEEEEE
    .word   __size__

_start2:
    // Elevate privileges if needed
    mrs     r0, cpsr
    tst     r0, #0xF
    moveq   r0, pc
    svceq   0x7B
    nop

    // Disable interrupts ASAP, clear flags, change sp
    msr     cpsr_cxsf, 0xD3
    ldr     sp, =0x08100000
    adr     r4, _p9TakeoverParams
    adr     r6, _start

    // Clean and invalidate the data cache, invalidate the instruction cache, drain the write buffer
    mov     r0, #0
    ldr     r12, =0xFFFF0830
    blx     r12
    mcr     p15, 0, r0, c7, c5, 0
    mcr     p15, 0, r0, c7, c10, 4

    // Disable caches / MPU
    mrc     p15, 0, r0, c1, c0, 0   // read control register
    bic     r0, #(1<<12)            // - instruction cache disable
    bic     r0, #(1<<2)             // - data cache disable
    bic     r0, #(1<<0)             // - mpu disable
    mcr     p15, 0, r0, c1, c0, 0   // write control register
    add     r12, pc, #1
    bx      r12

.thumb
_start2_thumb:
    // Relocate (we hope that memcpy acts in a position-independent way)
    ldr     r5, =0x08028000
    mov     r0, r5
    mov     r1, r6
    ldr     r2, [r4, #16]
    bl      memcpy

    ldr     r0, [r4]
    ldr     r1, [r4, #4]
    ldr     r2, [r4, #8]
    ldr     r3, [r4, #12]

    add     r5, r5, #(_start2_jmp_main - _start + 1)
    bx      r5

_start2_jmp_main:
    b       p9TakeoverMain
    b       .
.pool
.arm

_start1:
    // Elevate privileges if needed
    mrs     r0, cpsr
    tst     r0, #0xF
    moveq   r0, pc
    svceq   0x7B
    nop

    // Disable interrupts ASAP, clear flags
    msr     cpsr_cxsf, 0xD3

    // Change the stack pointer
    mov     sp, #0x08100000

    // Clean and invalidate the data cache, invalidate the instruction cache, drain the write buffer
    mov     r4, #0
    ldr     r12, =0xFFFF0830
    blx     r12
    mcr     p15, 0, r4, c7, c5, 0
    mcr     p15, 0, r4, c7, c10, 4

    // Disable caches / MPU
    mrc     p15, 0, r0, c1, c0, 0   // read control register
    bic     r0, #(1<<12)            // - instruction cache disable
    bic     r0, #(1<<2)             // - data cache disable
    bic     r0, #(1<<0)             // - mpu disable
    mcr     p15, 0, r0, c1, c0, 0   // write control register

    // Give read/write access to all the memory regions
    ldr     r0, =0x3333333
    mcr     p15, 0, r0, c5, c0, 2 // write data access
    mcr     p15, 0, r0, c5, c0, 3 // write instruction access

    // Set MPU permissions and cache settings
    ldr     r0, =0xFFFF001D // ffff0000 32k  | bootrom (unprotected part)
    ldr     r1, =0x01FF801D // 01ff8000 32k  | itcm
    ldr     r2, =0x08000029 // 08000000 2M   | arm9 mem (O3DS / N3DS)
    ldr     r3, =0x10000029 // 10000000 2M   | io mem (ARM9 / first 2MB)
    ldr     r4, =0x20000037 // 20000000 256M | fcram (O3DS / N3DS)
    ldr     r5, =0x1FF00027 // 1FF00000 1M   | dsp / axi wram
    ldr     r6, =0x1800002D // 18000000 8M   | vram (+ 2MB)
    mov     r7, #0
    mov     r8, #5
    mcr     p15, 0, r0, c6, c0, 0
    mcr     p15, 0, r1, c6, c1, 0
    mcr     p15, 0, r2, c6, c2, 0
    mcr     p15, 0, r3, c6, c3, 0
    mcr     p15, 0, r4, c6, c4, 0
    mcr     p15, 0, r5, c6, c5, 0
    mcr     p15, 0, r6, c6, c6, 0
    mcr     p15, 0, r7, c6, c7, 0
    mcr     p15, 0, r8, c3, c0, 0   // Write bufferable 0, 2
    mcr     p15, 0, r8, c2, c0, 0   // Data cacheable 0, 2
    mcr     p15, 0, r8, c2, c0, 1   // Inst cacheable 0, 2

    // Relocate ourselves
    adr     r0, _start
    ldr     r1, =__start__
    ldr     r2, =__end__
    _relocate_loop:
        ldmia       r0!, {r3-r10}
        stmia       r1!, {r3-r10}
        cmp         r1, r2
        blo         _relocate_loop

    // Enable caches / MPU / ITCM
    mrc     p15, 0, r0, c1, c0, 0   // read control register
    orr     r0, r0, #(1<<18)        // - ITCM enable
    orr     r0, r0, #(1<<13)        // - alternate exception vectors enable
    orr     r0, r0, #(1<<12)        // - instruction cache enable
    orr     r0, r0, #(1<<2)         // - data cache enable
    orr     r0, r0, #(1<<0)         // - mpu enable
    mcr     p15, 0, r0, c1, c0, 0   // write control register

    ldr     lr, =_finalJump
    ldr     r12, =arm9main
    bx      r12

_finalJump:
    // Flush caches
    ldr     r12, =0xFFFF0830
    blx     r12

    // Disable MPU
    ldr     r0, =0x42078            // alt vector select, enable itcm
    mcr     p15, 0, r0, c1, c0, 0

    mov     lr, #0
    mov     r0, #0
    ldr     r2, =#0x23F00000
    bx      r2
