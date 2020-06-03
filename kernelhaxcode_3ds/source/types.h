#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

/// The maximum value of a u64.
#define U64_MAX	UINT64_MAX

/// would be nice if newlib had this already
#ifndef SSIZE_MAX
#ifdef SIZE_MAX
#define SSIZE_MAX ((SIZE_MAX) >> 1)
#endif
#endif

typedef uint8_t u8;   ///<  8-bit unsigned integer
typedef uint16_t u16; ///< 16-bit unsigned integer
typedef uint32_t u32; ///< 32-bit unsigned integer
typedef uint64_t u64; ///< 64-bit unsigned integer

typedef int8_t s8;   ///<  8-bit signed integer
typedef int16_t s16; ///< 16-bit signed integer
typedef int32_t s32; ///< 32-bit signed integer
typedef int64_t s64; ///< 64-bit signed integer

typedef volatile u8 vu8;   ///<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; ///< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; ///< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; ///< 64-bit volatile unsigned integer.

typedef volatile s8 vs8;   ///<  8-bit volatile signed integer.
typedef volatile s16 vs16; ///< 16-bit volatile signed integer.
typedef volatile s32 vs32; ///< 32-bit volatile signed integer.
typedef volatile s64 vs64; ///< 64-bit volatile signed integer.

typedef u32 Result;

/// Creates a bitmask from a bit number.
#define BIT(n) (1U<<(n))

/// Aligns a struct (and other types?) to m, making sure that the size of the struct is a multiple of m.
#define ALIGN(m)   __attribute__((aligned(m)))
/// Packs a struct (and other types?) so it won't include padding bytes.
#define PACKED     __attribute__((packed))

/// Packs a system version from its components.
#define SYSTEM_VERSION(major, minor, revision) \
    (((major)<<24)|((minor)<<16)|((revision)<<8))

#define TRY(expr)   if((res = (expr)) & 0x80000000) return res;

#define REG8(a)     (*(vu8 *)(a))
#define REG16(a)    (*(vu16 *)(a))
#define REG32(a)    (*(vu32 *)(a))

#define MAP_ADDR    0x40000000

// PXI regs at +0xA1000
#define LCD_REGS_BASE           (MAP_ADDR + 0xA0000)
#define CFG11_REGS_BASE         (MAP_ADDR + 0xA2000)

//#define CFG11_SOCINFO           REG16(CFG11_REGS_BASE + 0x0FFC)
#define CFG11_DSP_CNT           REG8(CFG11_REGS_BASE + 0x1230)

#define KERNEL_VERSION_MAJOR    REG8(0x1FF80003)
#define KERNEL_VERSION_MINOR    REG8(0x1FF80002)
#define KERNPA2VA(a)            ((a) + (KERNEL_VERSION_MINOR < 44 ? 0xD0000000 : 0xC0000000))
#define IS_N3DS                 (*(vu32 *)0x1FF80030 >= 6) // APPMEMTYPE. Hacky but doesn't use APT. Handles O3DS fw running on N3DS

typedef struct TakeoverParameters {
    u64 firmTid;
    u8 kernelVersionMajor;
    u8 kernelVersionMinor;
    bool isN3ds;
    size_t payloadFileOffset;
    char payloadFileName[255+1];
} TakeoverParameters;

extern TakeoverParameters g_takeoverParameters;

static inline void lcdDebug(bool topScreen, u32 r, u32 g, u32 b)
{
    u32 base = topScreen ? LCD_REGS_BASE + 0x200 : LCD_REGS_BASE + 0xA00;
    *(vu32 *)(base + 4) = BIT(24) | b << 16 | g << 8 | r;
}

