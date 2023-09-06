/*
 * Copyright (c) 2004-2023 Mark Burkley.
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
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <GL/glut.h>
#include <GL/gl.h>

#include "vdp.h"
#include "grom.h"
#include "trace.h"
#include "status.h"

#define VDP_READ 1
#define VDP_WRITE 2

#define VDP_BITMAP_MODE     (vdp.reg[0] & 0x02)
#define VDP_EXTERNAL        (vdp.reg[0] & 0x01)

#define VDP_16K             (vdp.reg[1] & 0x80)
#define VDP_SCRN_ENABLE     (vdp.reg[1] & 0x40)
#define VDP_INT_ENABLE      (vdp.reg[1] & 0x20)
#define VDP_TEXT_MODE       (vdp.reg[1] & 0x10)
#define VDP_MULTI_MODE      (vdp.reg[1] & 0x08)
#define VDP_SPRITESIZE      (vdp.reg[1] & 0x02)
#define VDP_SPRITEMAG       (vdp.reg[1] & 0x01)

#define VDP_SCRN_IMGTAB     ((vdp.reg[2] & 0x0F) << 10)

#define VDP_GR_COLTAB_ADDR  (vdp.reg[3] << 6)
#define VDP_BM_COLTAB_ADDR  ((vdp.reg[3] & 0x80) << 6)
#define VDP_BM_COLTAB_SIZE  ((vdp.reg[3] & 0x7F) << 6 || 0x3F)

#define VDP_CHARPAT_TAB     ((vdp.reg[4] & 0x07) << 11)

#define VDP_SPRITEATTR_TAB  ((vdp.reg[5] & 0x7F) << 7)

#define VDP_SPRITEPAT_TAB   ((vdp.reg[6] & 0x07) << 11)

#define VDP_FG_COLOUR       ((vdp.reg[7] & 0xf0) >> 4)
#define VDP_BG_COLOUR       (vdp.reg[7] & 0x0f)

#define MAX_ADDR 0x4000

// #include "cpu.h"

#define VDP_STATUS_PANE_WIDTH 32

// #define VDP_SCALE   4
#define VDP_XSIZE 256
#define VDP_YSIZE 192

static struct
{
   int r;
   int g;
   int b;
}
colours[16] =
{
    {0x00, 0x00, 0x00}, // Transparent
    {0x00, 0x00, 0x00},
    {0x00, 0xc0, 0x00},
    {0x40, 0xff, 0x40},
    {0x00, 0x00, 0x80},
    {0x40, 0x40, 0xfF},
    {0x80, 0x00, 0x00},
    {0x00, 0xff, 0xff},
    {0xc0, 0x00, 0x00},
    {0xff, 0x40, 0x40},
    {0x80, 0x80, 0x00},
    {0xff, 0xff, 0x40},
    {0x00, 0x80, 0x00},
    {0xff, 0x40, 0xff},
    {0xc0, 0xc0, 0xc0},
    {0xff, 0xff, 0xff}
};

struct
{
    WORD addr;
    int cmdInProg;
    int mode;
    BYTE reg[8];
    BYTE cmd;
    BYTE st;
    BYTE ram[MAX_ADDR];
    bool graphics;
}
vdp;

static bool vdpInitialised = false;
static bool vdpRefreshNeeded = false;

/*  The framebuffer is a 2D array of pixels with 4 bytes per pixel.  The first 3
 *  bytes of each pixel are r, g, b respectively and the 4th is not
 *  used.  The framebuffer is increased in size by the pixel magnification
 *  factor and also if a status pane is displayed.  Since these are
 *  configurable, the framebuffer is allocated at runtime.
 */
static struct _frameBuffer
{
    BYTE r;
    BYTE g;
    BYTE b;
    BYTE unused;
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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

int vdpRead (int addr, int size)
{
    if (size != 1)
    {
        halt ("VDP can only read bytes\n");
    }

    switch (addr)
    {
    case 0:
        if (vdp.addr >= MAX_ADDR)
        {
            halt ("VDP read out of range");
        }

        vdp.cmdInProg = 0;
        return vdp.ram[vdp.addr++];
    case 2:
        vdp.cmdInProg = 0;
        return vdp.st;
    default:
        halt ("VDP invalid address");
        break;
    }

    return 0;
}

void vdpWrite (int addr, int data, int size)
{
    if (size != 1)
    {
        halt ("VDP can only write bytes\n");
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

        vdp.ram[vdp.addr++] = data;
        vdpRefreshNeeded = true;

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
                vdp.mode = 0;
                vdp.reg[data&7] = vdp.cmd;
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
    glutCreateWindow("TI-99 emulator");

    vdpInitialised = 1;
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
        x >= frameBufferXSize * frameBufferScale || 
        x >= frameBufferYSize * frameBufferScale)
    {
        halt ("VDP coords out of range\n");
    }

    /*  Col 0 is transparent, use global background */
    if (col == 0)
        col = VDP_BG_COLOUR;

    /*  Draw a square block of pixels of size (scale x scale) */
    for (i = 0; i < frameBufferScale; i++)
        for (j = 0; j < frameBufferScale; j++)
            vdpPlotRaw (x*frameBufferScale+i, y*frameBufferScale+j, col);
}

/*  Draw an 8x8 character on screen.  cx and cy are the column (0 to 31) and row
 *  (0 to 23).  ch is the character index (0 to 255).  Values are multiplied by
 *  8 (<< 3) to convert to pixel coords.
 */
static void vdpDrawChar (int cx, int cy, int ch)
{
    int x, y;
    int data;
    int charpat = VDP_CHARPAT_TAB + (ch << 3);
    int colour = vdp.ram[VDP_GR_COLTAB_ADDR + (ch >> 3)];

    for (y = 0; y < 8; y++)
    {
        data = vdp.ram[charpat+y];

        for (x = 0; x < 8; x++)
        {
            vdpPlot ((cx << 3) + x, (cy << 3) + y,
                     (data & 0x80) ? (colour >> 4) : (colour & 0x0F));
            data <<= 1;
        }
    }
}

static void vdpDrawByte (int data, int x, int y, int col)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            vdpPlot (x + i, y, col);
        }

        data <<= 1;
    }
}

static void vdpDrawByteMagnified (int data, int x, int y, int col)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            vdpPlot (x+i*2, y, col);
            vdpPlot (x+i*2+1, y, col);
        }

        data <<= 1;
    }
}

static void vdpDrawSprites8x8 (int x, int y, int p, int c)
{
    int i;

    mprintf (LVL_VDP, "Draw sprite pat 8x8 %d at %d,%d\n", p, x, y);
    for (i = 0; i < 8; i++)
    {
        vdpDrawByte (vdp.ram[p+i], x, y + i, c & 0x0F);
    }
}

static void vdpDrawSprites16x16 (int x, int y, int p, int c)
{
    int i, col;

    mprintf (LVL_VDP, "Draw sprite pat 16x16 %d at %d,%d\n", p, x, y);

    for (col = 0; col < 16; col += 8)
    {
        for (i = 0; i < 16; i++)
        {
            vdpDrawByte (vdp.ram[p+i+col*2], x + col, y + i, c & 0x0F);
        }
    }
}

static void vdpDrawSprites8x8Mag (int x, int y, int p, int c)
{
    int i;

    mprintf (LVL_VDP, "Draw sprite 8x8 mag pat %d at %d,%d\n", p, x, y);

    for (i = 0; i < 8; i++)
    {
        vdpDrawByteMagnified (vdp.ram[p+i], x, y + i*2, c & 0x0F);
        vdpDrawByteMagnified (vdp.ram[p+i], x, y + i*2+1, c & 0x0F);
    }
}

static void vdpDrawSprites16x16Mag (int x, int y, int p, int c)
{
    int i, col;

    mprintf (LVL_VDP, "Draw sprite 16x16 mag pat %d at %d,%d\n", p, x, y);

    for (col = 0; col < 16; col += 8)
    {
        for (i = 0; i < 16; i++)
        {
            vdpDrawByteMagnified (vdp.ram[p+i+col*2], x + col, y + i*2, c & 0x0F);
            vdpDrawByteMagnified (vdp.ram[p+i+col*2], x + col, y + i*2+1, c & 0x0F);
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

    size = (VDP_SPRITESIZE ? 1 : 0);
    size |= (VDP_SPRITEMAG ? 2 : 0);

    for (i = 0; i < 32; i++)
    {
        y = vdp.ram[attr + i*4] + 1;
        x = vdp.ram[attr + i*4 + 1];
        p = vdp.ram[attr + i*4 + 2] * entrySize + VDP_SPRITEPAT_TAB;
        c = vdp.ram[attr + i*4 + 3];

        if (y == 0xD0)
        {
            mprintf (LVL_VDP, "Sprite %d switched off\n", y);
            return;
        }

        mprintf (LVL_VDP, "Draw sprite %d @ %d,%d pat=%d, colour=%d\n", i, x, y, p, c);

        statusSpriteUpdate (i, x, y, p, c);

        /*  Transparent sprites don't get drawn */
        if (c == 0)
            continue;

        switch (size)
        {
        case 0: vdpDrawSprites8x8 (x, y, p, c); break;
        case 1: vdpDrawSprites16x16 (x, y, p, c); break;
        case 2: vdpDrawSprites8x8Mag (x, y, p, c); break;
        case 3: vdpDrawSprites16x16Mag (x, y, p, c); break;
        }
    }
}

void vdpRefresh (int force)
{
    int sc;

    if (!vdpRefreshNeeded || !vdpInitialised)
        return;

    vdpRefreshNeeded = false;

    if (VDP_BITMAP_MODE || VDP_TEXT_MODE || VDP_MULTI_MODE)
    {
        halt ("unsupported VDP mode");
    }

    for (sc = 0; sc < 0x300; sc++)
    {
        vdpDrawChar (sc % 32, sc / 32, vdp.ram[VDP_SCRN_IMGTAB + sc]);
    }

    vdpDrawSprites ();
    statusPaneDisplay ();
    vdpScreenUpdate();
}

