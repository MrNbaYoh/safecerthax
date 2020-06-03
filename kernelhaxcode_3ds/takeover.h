#pragma once

// For usage by the calling code

#ifndef KERNEL_VERSION_MAJOR
#define KERNEL_VERSION_MAJOR    (*(vu8 *)0x1FF80003)
#endif

#ifndef KERNEL_VERSION_MINOR
#define KERNEL_VERSION_MINOR    (*(vu8 *)0x1FF80002)
#endif

#ifndef KERNPA2VA
#define KERNPA2VA(a)            ((a) + (KERNEL_VERSION_MINOR < 44 ? 0xD0000000 : 0xC0000000))
#endif

#ifndef IS_N3DS
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT
#endif

#ifndef SYSTEM_VERSION
/// Packs a system version from its components.
#define SYSTEM_VERSION(major, minor, revision) \
    (((major)<<24)|((minor)<<16)|((revision)<<8))
#endif

#define KHC3DS_MAP_ADDR         0x40000000

typedef struct BlobLayout {
    u8 padding0[0x1000]; // to account for firmlaunch params in case we're placed at FCRAM+0
    u8 code[0x20000];
    u32 l2table[0x100];
    u32 padding[0x400 - 0x100];
} BlobLayout;

static inline void khc3dsLcdDebug(bool topScreen, u32 r, u32 g, u32 b)
{
    u32 base = topScreen ? KHC3DS_MAP_ADDR + 0xA0200 : KHC3DS_MAP_ADDR + 0xA0A00;
    *(vu32 *)(base + 4) = BIT(24) | b << 16 | g << 8 | r;
}

static inline void khc3dsPrepareL2Table(BlobLayout *layout)
{
    u32 *l2table = layout->l2table;

    u32 vaddr = (u32)layout->code;
    u32 paddr = vaddr;

#ifndef KHC3DS_NO_ADDR_TRANSLATION
    switch (vaddr) {
        case 0x14000000 ... 0x1C000000 - 1: paddr = vaddr + 0x0C000000; break; // LINEAR heap
        case 0x30000000 ... 0x40000000 - 1: paddr = vaddr - 0x10000000; break; // v8.x+ LINEAR heap
        default: paddr = 0; break; // should never be reached
    }
#endif

    // Map AXIWRAM RWX RWX Shared
    // Outer Write-Through cached, No Allocate on Write, Buffered
    // Inner Cached Write-Back Write-Allocate, Buffered
    // DCache is PIPT
    for(u32 offset = 0; offset < 0x80000; offset += 0x1000) {
        l2table[offset >> 12] = (0x1FF80000 + offset) | 0x5B6;
    }

    // Map the code buffer cacheable
    // ICache is VIPT, but we should be fine as the only other mapping is XN and
    // shouldn't have filled any instruction cache line...
    for(u32 offset = 0; offset < sizeof(layout->code); offset += 0x1000) {
        l2table[(0x80000 + offset) >> 12] = (paddr + offset) | 0x5B6;
    }

    // LCD registers (for debug), RW, Shared, Device
    l2table[0xA0000 >> 12] = 0x10202000 | 0x437;

    // PXI registers (for cleaner hooks)
    l2table[0xA1000 >> 12] = 0x10163000 | 0x437;

    // CFG11 registers, two pages
    l2table[0xA2000 >> 12] = 0x10140000 | 0x437;
    l2table[0xA3000 >> 12] = 0x10141000 | 0x437;
}

static inline Result khc3dsTakeover(const char *payloadFileName, size_t payloadFileOffset)
{
    u64 firmTidMask = IS_N3DS ? 0x0004013820000000ULL : 0x0004013800000000ULL;
    u8 kernelVersionMinor = KERNEL_VERSION_MINOR;
    u32 firmTidLow;

    switch (kernelVersionMinor) {
        // Up to 11.2: use safehax
        case 0 ... 53 - 1:
            firmTidLow = 3;
            break;
        // 11.3: use safehax v1.1
        case 53:
            firmTidLow = 2;
            break;
        default:
            // No exploit
            khc3dsLcdDebug(true, 0, 255, 255);
            return 0xDEADFFFF;
    }

    // DSB, Flush Prefetch Buffer (more or less "isb")
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");

    return ((Result (*)(u64, const char *, size_t))(KHC3DS_MAP_ADDR + 0x80000))(firmTidMask | firmTidLow, payloadFileName, payloadFileOffset);
}

static inline Result khc3dsTakeoverWithArm9Pwned(const char *payloadFileName, size_t payloadFileOffset)
{
    u64 firmTid = 0xFFFFFFFF;
    // DSB, Flush Prefetch Buffer (more or less "isb")
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" :: "r" (0) : "memory");
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c5, 4" :: "r" (0) : "memory");

    return ((Result (*)(u64, const char *, size_t))(KHC3DS_MAP_ADDR + 0x80000))(firmTid, payloadFileName, payloadFileOffset);
}
