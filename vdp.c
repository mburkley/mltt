#include <stdio.h>
#include <time.h>
#include <stdlib.h>
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
#include "trace.h"

#define VDP_READ 1
#define VDP_WRITE 2
#define SCALE   4

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

#define MAX_ADDR 0x4000

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
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x30, 0x00},
    {0x10, 0x3f, 0x10},
    {0x00, 0x00, 0x20},
    {0x10, 0x10, 0x3F},
    {0x20, 0x00, 0x00},
    {0x00, 0x3f, 0x3f},
    {0x30, 0x00, 0x00},
    {0x3f, 0x10, 0x10},
    {0x20, 0x20, 0x00},
    {0x3f, 0x3f, 0x10},
    {0x00, 0x20, 0x00},
    {0x3f, 0x10, 0x3f},
    {0x30, 0x30, 0x30},
    {0x3f, 0x3f, 0x3f}
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
    BOOL graphics;
}
vdp;
static int plots;
static unsigned char frameBuffer[VDP_YSIZE*SCALE][VDP_XSIZE*SCALE][4];
static int vdpInitialised = 0;
static int vdpRefreshNeeded = false;

static void vdpScreenUpdate (void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawPixels (VDP_XSIZE*SCALE, VDP_YSIZE*SCALE, GL_RGBA, GL_UNSIGNED_BYTE, frameBuffer);
    glutSwapBuffers();
}

void vdpStatus (void)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        printf ("R%d:%02X ", i, vdp.reg[i]);
    }

    printf ("\nSt:%02X\n", vdp.st);
}

int vdpRead (int addr, int size)
{
    // mprintf(LVL_VDP, "%s %x\n", __func__, addr);
    if (size != 1)
    {
        halt ("VDP can only read bytes\n");
    }

    switch (addr)
    {
    case 0:
        // tms9900.ram->b[0x8801] = vdpReadData ();
        if (vdp.addr >= MAX_ADDR)
        {
            halt ("VDP read out of range");
        }

        vdp.cmdInProg = 0;
        // mprintf (LVL_VDP, "VDP : [%04X] -> %02X\n", vdp.addr, vdp.ram[vdp.addr]);
        return vdp.ram[vdp.addr++];
    case 2:
        // tms9900.ram->b[0x8803] =
        // mprintf (LVL_VDP, "VDP : status -> %02X\n", vdp.st);
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
    // mprintf(LVL_VDP, "%s %x=%x\n", __func__, addr, data);
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

        mprintf (LVL_VDP, "VDP : %02X -> [%04X] ", data, vdp.addr);

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
            // mprintf (LVL_VDP, "\n** VDP **  command -> %02X/%02X\n", vdp.cmd, data);

            switch (data >> 6)
            {
            case 0:
                vdp.mode = VDP_READ;
                vdp.addr = ((data & 0x3F) << 8) | vdp.cmd;
                // mprintf (LVL_VDP, "\n** VDP **  read addr -> %04X\n", vdp.addr);
                break;

            case 1:
                vdp.mode = VDP_WRITE;
                vdp.addr = ((data & 0x3F) << 8) | vdp.cmd;
                // mprintf (LVL_VDP, "\n** VDP **  write addr -> %04X\n", vdp.addr);
                break;

            case 2:
                // mprintf (LVL_VDP, "\n** VDP **  cmd? -> %04X\n", vdp.addr);
                vdp.mode = 0;
                vdp.reg[data&7] = vdp.cmd;
                // vdpStatus ();
                break;
            }
        }
        else
        {
            // mprintf (LVL_VDP, "\n** VDP ** interim -> %02X\n", data);
            vdp.cmd = data;
        }

        vdp.cmdInProg = !vdp.cmdInProg;
        break;
    default:
        halt ("VDP invalid address");
        break;
    }
}

void vdpInitGraphics (void)
{
    int argc=1;
    char *argv[] = { "foo" };
    glutInit(&argc, argv);
    glutInitWindowPosition(10,10);
    glutInitWindowSize(VDP_XSIZE*SCALE, VDP_YSIZE*SCALE);
    glutCreateWindow("TI-99 emulator");
    // fill(XSIZE, YSIZE);
    // glutMainLoop();
    vdpInitialised = 1;
}

static void vdpPlot (int x, int y, int col)
{
    // if (!vdp.graphics)
    //     return;

    // int offset = ((y * VDP_XSIZE) + x) * 4;

    int i, j;

    if (x < 0 || y < 0 || x >= VDP_XSIZE || y >= VDP_YSIZE)
        return;

    y = VDP_YSIZE - y - 1;

    for (i = 0; i < SCALE; i++)
        for (j = 0; j < SCALE; j++)
        {
            frameBuffer[y*SCALE+i][x*SCALE+j][0] = colours[col].r * 4;
            frameBuffer[y*SCALE+i][x*SCALE+j][1] = colours[col].g * 4;
            frameBuffer[y*SCALE+i][x*SCALE+j][2] = colours[col].b * 4;
            frameBuffer[y*SCALE+i][x*SCALE+j][3] = 0;
            plots++;
        }
}

static void vdpDrawChar (int cx, int cy, int ch)
{
    int x, y;
    int data;
    int charpat = VDP_CHARPAT_TAB + (ch << 3);
    int col = vdp.ram[VDP_GR_COLTAB_ADDR + (ch >> 3)];
// mprintf(LVL_VDP, "%d",ch);
    for (y = 0; y < 8; y++)
    {
        data = vdp.ram[charpat + y];

        for (x = 0; x < 8; x++)
        {
            vdpPlot ((cx << 3) + x, (cy << 3) + y,
                     (data & 0x80) ? (col >> 4) : (col & 0x0F));
            data <<= 1;
        }
    }
}

static void vdpDrawByte (int data, int x, int y, int col)
{
    int x1;

    for (x1 = x; x1 < (x + 8); x1++)
    {
        // mprintf (LVL_VDP, "SPLOT : %d,%d (col %d) \n", x1, y1, col);

        if (data & 0x80)
        {
            vdpPlot (x1, y, col);
        }

        data <<= 1;
    }

    // VDP_SPRITEMAG
    // VDP_SPRITESIZE)
}

static void vdpDrawSprites8x8 (int x, int y, int p, int c)
{
    // int pat = VDP_SPRITEPAT_TAB + p * 8;
    int y1;
    // int count = 0;

    for (y1 = 0; y1 < 8; y1++)
    {
        // mprintf (LVL_VDP, "Draw byte y=%d\n", y1);

        // if (count++ > 256)
        // {
        //     halt ("Sprites taking too long\n");
        // }

        vdpDrawByte (vdp.ram[p+y1], x, y + y1, c & 0x0F);

        // mprintf (LVL_VDP, "Drawn byte y=%d\n", y1);
    }
}

static void vdpDrawSprites16x16 (int x, int y, int p, int c)
{
    // int pat = VDP_SPRITEPAT_TAB + p * 8; // 32;
    int y1, col;
    // int count = 0;

    mprintf (LVL_VDP, "Draw sprite pat %d at %d,%d\n", p, x, y);

    for (col = 0; col < 16; col += 8)
    {
        for (y1 = 0; y1 < 16; y1++)
        {
            // mprintf (LVL_VDP, "Draw byte y=%d\n", y1);

            // if (count++ > 256)
            // {
            //     halt ("Sprites taking too long\n");
            // }

            vdpDrawByte (vdp.ram[p+y1+col*2], x + col, y + y1, c & 0x0F);

            // mprintf (LVL_VDP, "Drawn byte y=%d\n", y1);
        }
    }
}

static void vdpDrawSprites (void)
{
    int size;
    int i;
    int x, y, p, c;
    int attr = VDP_SPRITEATTR_TAB;

    size = (VDP_SPRITESIZE ? 1 : 0);
    size |= (VDP_SPRITEMAG ? 2 : 0);

    for (i = 0; i < 32; i++)
    {
        // mprintf (LVL_VDP, ">> i is %d\n", i);
        y = vdp.ram[attr + i*4] + 1;
        x = vdp.ram[attr + i*4 + 1];
        p = vdp.ram[attr + i*4 + 2] * 8 + VDP_SPRITEPAT_TAB;
        c = vdp.ram[attr + i*4 + 3];

        if (y == 0xD0 || y == 0xD1)
        {
            mprintf (LVL_VDP, "Sprite %d switched off\n", y);
            return;
        }

        mprintf (LVL_VDP, "Draw sprite %d @ %d,%d pat=%d, colour=%d\n", i, x, y, p, c);

        switch (size)
        {
        case 0: vdpDrawSprites8x8 (x, y, p, c); break;
        case 1: vdpDrawSprites16x16 (x, y, p, c); break;
        // case 2: vdpDrawSprites8x8Mag (x, y, p, c); break;
        // case 3: vdpDrawSprites16x16Mag (x, y, p, c); break;
        }
    }
}

void vdpRefresh (int force)
{
    // static time_t lastTime;
    // static int count;
    // static unsigned long lastTime;
    // static int count;
    // time_t t;
    // unsigned long t;
    int sc;

    // t = *(long*)0x46C;

    #if 0
    t = time(NULL);

    if (!force && t == lastTime)
        return;

    lastTime = t;

    #else
    // count++;

    // if (count < 6)
    //     return;

    if (!vdpRefreshNeeded)
        return;

    vdpRefreshNeeded = false;
    // count = 0;
    #endif

    // gromIntegrity();
    // mprintf (LVL_VDP, "VDP : refresh %d start\n", count);

    // mprintf (LVL_VDP, "VDP : refresh %d check mode\n", count);

    if (VDP_BITMAP_MODE || VDP_TEXT_MODE || VDP_MULTI_MODE)
    {
        halt ("unsupported VDP mode");
    }

    // mprintf (LVL_VDP, "VDP : refresh %d draw sc\n", count);

    for (sc = 0; sc < 0x300; sc++)
    {
        vdpDrawChar (sc % 32, sc / 32, vdp.ram[VDP_SCRN_IMGTAB + sc]);
    }

    vdpDrawSprites ();

     // mprintf (LVL_VDP, "VDP : refresh %d done, plots=%d\n", count++, plots);
    // gromIntegrity();

    if (vdpInitialised)
        vdpScreenUpdate();
}

