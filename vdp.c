/*
 * Copyright (c) 2004-2024 Mark Burkley.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <GL/glut.h>
#include <GL/gl.h>

#include "types.h"
#include "vdp.h"
#include "cru.h"
#include "grom.h"
#include "trace.h"
#include "status.h"
#include "interrupt.h"

#define VDP_READ 1
#define VDP_WRITE 2

#define VDP_BITMAP_MODE     (vdp.reg[0] & 0x02)
#define VDP_EXTERNAL        (vdp.reg[0] & 0x01)

#define VDP_16K             (vdp.reg[1] & 0x80)
#define VDP_SCRN_ENABLE     (vdp.reg[1] & 0x40) // Ignored for now
#define VDP_INT_ENABLE      (vdp.reg[1] & 0x20)
#define VDP_TEXT_MODE       (vdp.reg[1] & 0x10)
#define VDP_MULTI_MODE      (vdp.reg[1] & 0x08)
#define VDP_SPRITESIZE      (vdp.reg[1] & 0x02)
#define VDP_SPRITEMAG       (vdp.reg[1] & 0x01)

// FF=>x3c00, 06=>x1800
#define VDP_SCRN_IMGTAB     ((vdp.reg[2] & 0x0F) << 10)

#define VDP_GR_COLTAB_ADDR  (vdp.reg[3] << 6)

#define VDP_GR_CHARPAT_TAB  ((vdp.reg[4] & 0x07) << 11)

// FF=>addr=x2000, size=x1fff
#define VDP_BM_COLTAB_ADDR  ((vdp.reg[3] & 0x80) << 6)
#define VDP_BM_COLTAB_SIZE  (((vdp.reg[3] & 0x7F) << 6) | 0x3F)

// 03=>tab=0,size=x1fff
#define VDP_BM_CHARPAT_TAB  ((vdp.reg[4] & 0x04) << 11)
#define VDP_BM_CHARPAT_SIZE (((vdp.reg[4] & 0x03) << 11) | 0x7ff)

#define VDP_SPRITEATTR_TAB  ((vdp.reg[5] & 0x7F) << 7)

#define VDP_SPRITEPAT_TAB   ((vdp.reg[6] & 0x07) << 11)

#define VDP_FG_COLOUR       ((vdp.reg[7] & 0xf0) >> 4)
#define VDP_BG_COLOUR       (vdp.reg[7] & 0x0f)

#define VDP_VERT_RETRACE        0x80
#define VDP_SPRITE_LINE         0x40
#define VDP_SPRITE_COINC        0x20

#define MAX_ADDR 0x8002

#define VDP_STATUS_PANE_WIDTH 32

#define VDP_XSIZE 256
#define VDP_YSIZE 192

/* Values take from https://en.wikipedia.org/wiki/TMS9918 */
static struct
{
   int r;
   int g;
   int b;
}
colours[16] =
{
    {0x00, 0x00, 0x00}, // Transparent
    {0x00, 0x00, 0x00}, // blank
    {0x0a, 0xad, 0x1e}, // medium green
    {0x34, 0xc8, 0x4c}, // light green
    {0x2b, 0x2d, 0xe3}, // dark blue
    {0x51, 0x4b, 0xfb}, // light blue
    {0xbd, 0x29, 0x25}, // dark red
    {0x1e, 0xe2, 0xef}, // cyan
    {0xfb, 0x2c, 0x2b}, // medium red
    {0xff, 0x5f, 0x4c}, // light red
    {0xbd, 0xa2, 0x2b}, // dark yellow
    {0xd7, 0xb4, 0x54}, // light yellow
    {0x0a, 0x8c, 0x18}, // dark green
    {0xaf, 0x32, 0x9a}, // magenta
    {0xb2, 0xb2, 0xb2}, // grey
    {0xff, 0xff, 0xff}  // white
};

struct
{
    uint16_t addr;
    int cmdInProg;
    int mode;
    uint8_t reg[8];
    uint8_t cmd;
    uint8_t st;
    uint8_t ram[MAX_ADDR];
    bool graphics;
}
vdp;

static uint8_t vdpScreen[VDP_XSIZE][VDP_YSIZE];
static bool vdpSpriteCoinc[VDP_XSIZE][VDP_YSIZE];
static int vdpSpritesPerLine[VDP_YSIZE];
static bool vdpInitialised = false;
static bool vdpRefreshNeeded = false;
static bool vdpModeChanged = false;
static bool vdpSpritesEnabled = false;

/*  The framebuffer is a 2D array of pixels with 4 bytes per pixel.  The first 3
 *  bytes of each pixel are r, g, b respectively and the 4th is not
 *  used.  The framebuffer is increased in size by the pixel magnification
 *  factor and also if a status pane is displayed.  Since these are
 *  configurable, the framebuffer is allocated at runtime.
 */
static struct _frameBuffer
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t unused;
}
*frameBuffer;

static int frameBufferXSize;
static int frameBufferYSize;
static int frameBufferScale;

static inline struct _frameBuffer* pixel (int x, int y)
{
    return &frameBuffer[y*frameBufferXSize+x];
}

static void vdpScreenUpdate (void)
{
    glDrawPixels (frameBufferXSize, frameBufferYSize, GL_RGBA, GL_UNSIGNED_BYTE, frameBuffer);
    glutSwapBuffers();
}

int vdpReadStatus (void)
{
    return vdp.st;
}

int vdpReadRegister (int reg)
{
    return vdp.reg[reg];
}

uint16_t vdpRead (uint8_t *ptr, uint16_t addr, int size)
{
    uint16_t ret;

    if (size != 1)
    {
        halt ("VDP can only read bytes\n");
    }

    switch (addr)
    {
    case 0:
        if (vdp.addr >= MAX_ADDR)
        {
            printf ("VDP read %04X out of range", vdp.addr);
            halt ("VDP read out of range");
        }

        vdp.cmdInProg = 0;
        return vdp.ram[vdp.addr++];
    case 2:
        vdp.cmdInProg = 0;
        mprintf (LVL_VDP, "VDP read status %02X\n", vdp.st);
        ret = vdp.st;
        vdp.st &= 0x1f; // Read resets status bits
        return ret;
    default:
        printf ("addr=%04X\n", addr);
        halt ("VDP invalid address");
        break;
    }

    return 0;
}

void vdpWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size)
{
    uint8_t reg;

    if (size != 1)
    {
        data>>=8;
    }

    switch (addr)
    {
    case 0:
        if (vdp.addr >= MAX_ADDR)
        {
            halt ("VDP write out of range");
        }

        vdp.cmdInProg = 0;

        if (vdp.addr > 0x3FFF)
        {
            halt ("VDP memory out of range");
        }

        mprintf (LVL_VDP, "GROM: %04X VDP: %02X -> [%04X] ", gromAddr(), data, vdp.addr);

        if (VDP_BITMAP_MODE)
        {
            #if 0  // TODO
            if ((vdp.addr >= VDP_GR_COLTAB_ADDR && vdp.addr < VDP_GR_COLTAB_ADDR + 0x20) ||
                (vdp.addr >= VDP_SCRN_IMGTAB && vdp.addr < VDP_SCRN_IMGTAB + 0x300) ||
                (vdp.addr >= VDP_GR_CHARPAT_TAB && vdp.addr < VDP_GR_CHARPAT_TAB + 0x800) ||
                (vdp.addr >= VDP_SPRITEATTR_TAB && vdp.addr < VDP_SPRITEATTR_TAB + 0x80) ||
                (vdp.addr >= VDP_SPRITEPAT_TAB && vdp.addr < VDP_SPRITEPAT_TAB + 0x400))
            #endif
                vdpRefreshNeeded = true;
        }else
        {
            if ((vdp.addr >= VDP_GR_COLTAB_ADDR && vdp.addr < VDP_GR_COLTAB_ADDR + 0x20) ||
                (vdp.addr >= VDP_SCRN_IMGTAB && vdp.addr < VDP_SCRN_IMGTAB + 0x300) ||
                (vdp.addr >= VDP_GR_CHARPAT_TAB && vdp.addr < VDP_GR_CHARPAT_TAB + 0x800) ||
                (vdp.addr >= VDP_SPRITEATTR_TAB && vdp.addr < VDP_SPRITEATTR_TAB + 0x80) ||
                (vdp.addr >= VDP_SPRITEPAT_TAB && vdp.addr < VDP_SPRITEPAT_TAB + 0x400))
                vdpRefreshNeeded = true;
        }

        vdp.ram[vdp.addr++] = data;

        int i;

        for (i = 0; i < 8; i++)
        {
            mprintf (LVL_VDP, "%s", (data & 0x80) ? "*" : " ");
            data <<= 1;
        }
        mprintf (LVL_VDP, "\n");
        break;
    case 2:
        if (vdp.cmdInProg)
        {
            switch (data >> 6)
            {
            case 0:
                vdp.mode = VDP_READ;
                vdp.addr = ((data & 0x3F) << 8) | vdp.cmd;
                break;

            case 1:
                vdp.mode = VDP_WRITE;
                vdp.addr = ((data & 0x3F) << 8) | vdp.cmd;
                break;

            case 2:
                reg = data & 7;
                vdp.mode = 0;

                if (reg == 1 && vdp.reg[reg] != vdp.cmd)
                    vdpModeChanged = true;

                vdp.reg[reg] = vdp.cmd;
                mprintf (LVL_VDP, "VDP R%d=%02X\n", reg, vdp.cmd);
                vdpRefreshNeeded = true;
                break;
            }
        }
        else
        {
            vdp.cmd = data;
        }

        vdp.cmdInProg = !vdp.cmdInProg;
        break;
    default:
        halt ("VDP invalid address");
        break;
    }
}

void vdpInitGraphics (bool statusPane, int scale)
{
    int argc=1;
    char *argv[] = { "foo" };
    glutInit(&argc, argv);
    glutInitWindowPosition(10,10);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);

    frameBufferXSize = (VDP_XSIZE * scale) + (statusPane ? VDP_STATUS_PANE_WIDTH * 8 : 0);
    frameBufferYSize = VDP_YSIZE * scale;
    frameBufferScale = scale;

    if (statusPane)
        statusPaneInit (VDP_STATUS_PANE_WIDTH * 8, frameBufferYSize,
                        VDP_XSIZE * scale);

    frameBuffer = calloc (frameBufferXSize * frameBufferYSize,
                          sizeof (struct _frameBuffer));

    if (frameBuffer == NULL)
        halt ("allocated frame buffer");

    printf ("FB size is %d x %d\n", frameBufferXSize, frameBufferYSize);
    glutInitWindowSize(frameBufferXSize, frameBufferYSize);
    glutCreateWindow("TI-99 emulator v" VERSION);

    vdpInitialised = true;
}

/*  Raw plot, doesn't do any scaling, expects absolute coords */
void vdpPlotRaw (int x, int y, int col)
{
    y = frameBufferYSize - y - 1;

    pixel (x, y)->r = colours[col].r;
    pixel (x, y)->g = colours[col].g;
    pixel (x, y)->b = colours[col].b;
}

/*  Scaling plot, scales up x and y and draws a square of pixels */
static void vdpPlot (int x, int y, int col)
{
    int i, j;

    if (x < 0 || y < 0 || 
        x >= VDP_XSIZE || 
        y >= VDP_YSIZE)
    {
        // halt ("VDP coords out of range\n");
        return;
    }

    /*  Col 0 is transparent, use global background */
    if (col == 0)
        col = VDP_BG_COLOUR;

    if (vdpScreen[x][y] == col)
        return;

    vdpScreen[x][y] = col;

    /*  Draw a square block of pixels of size (scale x scale) */
    for (i = 0; i < frameBufferScale; i++)
        for (j = 0; j < frameBufferScale; j++)
            vdpPlotRaw (x*frameBufferScale+i, y*frameBufferScale+j, col);
}

/*  Draw an 8x8 character on screen.  cx and cy are the column (0 to 31) and row
 *  (0 to 23).  ch is the character index (0 to 255).  Values are multiplied by
 *  8 (<< 3) to convert to pixel coords.  bits is 6 or 8 for text or graphics.
 */
static void vdpDrawChar (int cx, int cy, int bits, int ch)
{
    int x, y;

    for (y = 0; y < 8; y++)
    {
        int charpat;
        int colpat;

        if (VDP_BITMAP_MODE)
        {
            /*  In bitmap mode the screen is divided vertically into thirds.  Each
             *  third has its own char set.  Each character set is 0x800 (1<<11) in
             *  size.
             */
            int addr = (ch << 3) + ((cy >> 3) << 11) + y;
            // charpat = VDP_BM_CHARPAT_TAB + (ch << 3) + ((cy >> 3) << 11) + y;
            // colpat = VDP_BM_COLTAB_ADDR + (ch << 3) + ((cy >> 3) << 11) + y;
            charpat = VDP_BM_CHARPAT_TAB + (addr & VDP_BM_COLTAB_SIZE);
            colpat = VDP_BM_COLTAB_ADDR + (addr & VDP_BM_COLTAB_SIZE);
        }
        else
        {
            charpat = VDP_GR_CHARPAT_TAB + (ch << 3) + y;
            colpat = VDP_GR_COLTAB_ADDR + (ch >> 3);
        }

        int data = vdp.ram[charpat];
        int colour;

        if (VDP_TEXT_MODE)
            colour = vdp.reg[7];
        else
            colour = vdp.ram[colpat];

        for (x = cx * bits; x < (cx+1) * bits; x++)
        {
            vdpPlot (x, (cy << 3) + y,
                     (data & 0x80) ? (colour >> 4) : (colour & 0x0F));
            data <<= 1;
        }
    }
}

static bool maxSpritesPerLine (int y, int sprite)
{
    if (y < 0 || y >= VDP_YSIZE)
        return false;

    vdpSpritesPerLine[y]++;
    // printf("per line %d=%d\n", y, vdpSpritesPerLine[y]);

    // mprintf (LVL_VDP, "per line %d = %d\n", y, vdpSpritesPerLine[y]);
    /*  If this is the fifth sprite on this line, record its number and set the
     *  5 per line flag.  If higher than the fifth then just don't draw it
     *  but don't record any details*/
    if (vdpSpritesPerLine[y] == 5)
    {
        mprintf (LVL_VDP, "sprite %d is 5th on line %d\n", sprite, y);
        vdp.st = (vdp.st & 0xe0) | sprite;
        vdp.st |= VDP_SPRITE_LINE;
        return true;
    }
    else if (vdpSpritesPerLine[y] > 5)
        return true;

    return false;
}

static void vdpDrawByte (int data, int x, int y, int col, int sprite)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            if (x+i < 0 || y < 0 || 
                x+i >= VDP_XSIZE || 
                y >= VDP_YSIZE)
            {
                /*  This pixel of the sprite is not visible */
                continue;
            }

            if (vdpSpriteCoinc[x+i][y])
                vdp.st |= VDP_SPRITE_COINC;

            vdpSpriteCoinc[x+i][y] = true;

            vdpPlot (x + i, y, col);
        }

        data <<= 1;
    }
}

static void vdpDrawByteMagnified (int data, int x, int y, int col, int sprite)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            if (x+i*2 < 0 || y < 0 || 
                x+i*2 >= VDP_XSIZE-1 || 
                y >= VDP_YSIZE)
            {
                /*  This pixel of the sprite is not visible */
                continue;
            }

            if (vdpSpriteCoinc[x+i*2][y] || vdpSpriteCoinc[x+i*2+1][y])
                vdp.st |= VDP_SPRITE_COINC;

            vdpSpriteCoinc[x+i*2][y] = true;
            vdpSpriteCoinc[x+i*2+1][y] = true;

            vdpPlot (x+i*2, y, col);
            vdpPlot (x+i*2+1, y, col);
        }

        data <<= 1;
    }
}

static void vdpDrawSprites8x8 (int x, int y, int p, int c, int sprite)
{
    int i;

    mprintf (LVL_VDP, "Draw sprite pat 8x8 0x%x at %d,%d\n", p, x, y);
    for (i = 0; i < 8; i++)
    {
        if (maxSpritesPerLine (y+i, sprite))
            continue;

        vdpDrawByte (vdp.ram[p+i], x, y + i, c & 0x0F, sprite);
    }
}

static void vdpDrawSprites16x16 (int x, int y, int p, int c, int sprite)
{
    int i, col;

    mprintf (LVL_VDP, "Draw sprite pat 16x16 0x%x at %d,%d\n", p, x, y);

    for (i = 0; i < 16; i++)
    {
        if (maxSpritesPerLine (y+i, sprite))
            continue;

        for (col = 0; col < 16; col += 8)
        {
            vdpDrawByte (vdp.ram[p+i+col*2], x + col, y + i, c & 0x0F, sprite);
        }
    }
}

static void vdpDrawSprites8x8Mag (int x, int y, int p, int c, int sprite)
{
    int i;

    mprintf (LVL_VDP, "Draw sprite 8x8 mag pat 0x%x at %d,%d\n", p, x, y);

    for (i = 0; i < 8; i++)
    {
        if (maxSpritesPerLine (y+i*2, sprite))
            continue;

        vdpDrawByteMagnified (vdp.ram[p+i], x, y + i*2, c & 0x0F, sprite);

        if (maxSpritesPerLine (y+i*2+1, sprite))
            continue;

        vdpDrawByteMagnified (vdp.ram[p+i], x, y + i*2+1, c & 0x0F, sprite);
    }
}

static void vdpDrawSprites16x16Mag (int x, int y, int p, int c, int sprite)
{
    int i, col;

    mprintf (LVL_VDP, "Draw sprite 16x16 mag pat 0x%x at %d,%d\n", p, x, y);

    for (i = 0; i < 16; i++)
    {
        for (col = 0; col < 16; col += 8)
        {
            if (maxSpritesPerLine (y+i*2, sprite))
                continue;

            vdpDrawByteMagnified (vdp.ram[p+i+col*2], x + col, y + i*2, c & 0x0F, sprite);

            if (maxSpritesPerLine (y+i*2+1, sprite))
                continue;

            vdpDrawByteMagnified (vdp.ram[p+i+col*2], x + col, y + i*2+1, c & 0x0F, sprite);
        }
    }
}

static void vdpDrawSprites (void)
{
    int size;
    int i;
    int x, y, p, c;
    int attr = VDP_SPRITEATTR_TAB;
    int entrySize = 8;

    vdp.st &= ~VDP_SPRITE_COINC;
    memset (vdpSpriteCoinc, 0, VDP_XSIZE * VDP_YSIZE * sizeof (bool));

    size = (VDP_SPRITESIZE ? 1 : 0);
    size |= (VDP_SPRITEMAG ? 2 : 0);

    for (i = 0; i < VDP_YSIZE; i++)
        vdpSpritesPerLine[i] = 0;

    for (i = 0; i < 32; i++)
    {
        y = vdp.ram[attr + i*4] + 1;
        x = vdp.ram[attr + i*4 + 1];
        p = vdp.ram[attr + i*4 + 2] * entrySize + VDP_SPRITEPAT_TAB;
        c = vdp.ram[attr + i*4 + 3];

        if (y == 0xD1)
        {
            mprintf (LVL_VDP, "Sprite %d switched off\n", y);
            return;
        }

        if (c & 0x80)
            x -= 32;

        mprintf (LVL_VDP, "Draw sprite %d @ %d,%d pat=%d, colour=%d\n", i, x, y, p, c);

        statusSpriteUpdate (i, x, y, p, c);

        #if 0
        /*  Transparent sprites don't get drawn */
        if (c == 0)
            continue;
        #endif

        switch (size)
        {
        case 0: vdpDrawSprites8x8 (x, y, p, c, i); break;
        case 1: vdpDrawSprites16x16 (x, y, p, c, i); break;
        case 2: vdpDrawSprites8x8Mag (x, y, p, c, i); break;
        case 3: vdpDrawSprites16x16Mag (x, y, p, c, i); break;
        }
    }
}

static void vdpUpdateMode (void)
{
    /*  In text mode, the rightmost 16 pixels are set to the background colour
     */
    if (VDP_TEXT_MODE)
    {
        for (int x = VDP_XSIZE - 16; x < VDP_XSIZE; x++)
        {
            for (int y = 0; y < VDP_YSIZE; y++)
            {
                vdpPlot (x, y, 0);
            }
        }

        vdpSpritesEnabled = false;
    }
    else
        vdpSpritesEnabled = true;
}

void vdpRefresh (void)
{
    int sc;

    if (VDP_INT_ENABLE)
    {
        /*
         *  Clear bit 2 to indicate VDP interrupt
         */
        mprintf (LVL_VDP, "IRQ_VDP lowered\n");
        cruBitInput (0, IRQ_VDP, 0);
        vdp.st |= VDP_VERT_RETRACE;
    }

    if (!vdpRefreshNeeded || !vdpInitialised)
        return;

    vdpRefreshNeeded = false;

    if (vdpModeChanged)
    {
        vdpUpdateMode ();
        vdpModeChanged = false;
    }

    if (VDP_MULTI_MODE)
    {
        printf ("mode=%s\n",
        VDP_TEXT_MODE?"txt":(VDP_MULTI_MODE?"multi":"???"));
        halt ("unsupported VDP mode");
    }

    for (sc = 0; sc < (VDP_TEXT_MODE ? 0x3C0 : 0x300); sc++)
    {
        if (VDP_TEXT_MODE)
            vdpDrawChar (sc % 40, sc / 40, 6, vdp.ram[VDP_SCRN_IMGTAB + sc]);
        else
            vdpDrawChar (sc % 32, sc / 32, 8, vdp.ram[VDP_SCRN_IMGTAB + sc]);
    }

    if (vdpSpritesEnabled)
        vdpDrawSprites ();

    statusPaneDisplay ();
    vdpScreenUpdate();
}

