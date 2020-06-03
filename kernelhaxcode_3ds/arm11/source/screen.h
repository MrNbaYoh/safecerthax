#include "types.h"

#define LCD_REGS_BASE           0x10202000

#define LCD_TOP_FILL_REG        *(vu32 *)(LCD_REGS_BASE + 0x200 + 4)
#define LCD_BOTTOM_FILL_REG     *(vu32 *)(LCD_REGS_BASE + 0xA00 + 4)
#define LCD_FILL_ENABLE         (1u << 24)
#define LCD_FILL_COLOR(r,g,b)   ((r) | ((g) << 8) | ((b) << 16))
#define LCD_FILL(r,g,b)         (LCD_FILL_ENABLE | LCD_FILL_COLOR(r,g,b))

#define SCREEN_TOP_WIDTH     400
#define SCREEN_BOTTOM_WIDTH  320
#define SCREEN_HEIGHT        240
#define SCREEN_TOP_FBSIZE    (3 * SCREEN_TOP_WIDTH * SCREEN_HEIGHT)
#define SCREEN_BOTTOM_FBSIZE (3 * SCREEN_BOTTOM_WIDTH * SCREEN_HEIGHT)

struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
};

static const struct fb defaultFbs[2] = {
    {
        .top_left  = (u8 *)0x18300000,
        .top_right = (u8 *)0x18300000,
        .bottom    = (u8 *)0x18346500,
    },
    {
        .top_left  = (u8 *)0x18400000,
        .top_right = (u8 *)0x18400000,
        .bottom    = (u8 *)0x18446500,
    },
};

static const u32 defaultBrightnessLevel = 0x5F;

void prepareScreens(void);
void resetScreens(void);
