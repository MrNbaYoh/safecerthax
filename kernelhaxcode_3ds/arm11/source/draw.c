/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2018 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

/*
*   Code to print to the screen by mid-kid @CakesFW
*   https://github.com/mid-kid/CakesForeveryWan
*/

#include <stdarg.h>
#include <string.h>
#include "screen.h"
#include "draw.h"
#include "fmt.h"
#include "font.h"

void drawCharacter(bool isTopScreen, u32 posX, u32 posY, u32 color, char character)
{
    u8 *select = isTopScreen ? defaultFbs[0].top_left : defaultFbs[0].bottom;

    for(u32 y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for(u32 x = 0; x < 8; x++)
            if(((charPos >> (7 - x)) & 1) == 1)
            {
                u32 screenPos = (posX * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - y - posY - 1) * 3) + x * 3 * SCREEN_HEIGHT;

                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
    }
}

u32 drawString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *string)
{
    for(u32 i = 0, line_i = 0; i < strlen(string); i++)
        switch(string[i])
        {
            case '\n':
                posY += SPACING_Y;
                line_i = 0;
                break;

            case '\t':
                line_i += 2;
                break;

            default:
                //Make sure we never get out of the screen
                if(line_i >= ((isTopScreen ? SCREEN_TOP_WIDTH : SCREEN_BOTTOM_WIDTH) - posX) / SPACING_X)
                {
                    posY += SPACING_Y;
                    line_i = 1; //Little offset so we know the same string continues
                    if(string[i] == ' ') break; //Spaces at the start look weird
                }

                drawCharacter(isTopScreen, posX + line_i * SPACING_X, posY, color, string[i]);

                line_i++;
                break;
        }

    return posY;
}

u32 drawFormattedStringV(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *fmt, va_list args)
{
    char buf[DRAW_MAX_FORMATTED_STRING_SIZE + 1];
    xvsprintf(buf, fmt, args);

    return drawString(isTopScreen, posX, posY, color, buf);
}

u32 drawFormattedString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    posY = drawFormattedStringV(isTopScreen, posX, posY, color, fmt, args);
    va_end(args);

    return posY;
}
