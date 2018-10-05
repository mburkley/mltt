#include <stdio.h>
#include <time.h>
#include <dos.h>
// #include <graphics.h>

#include "vdp.h"

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

#define MAX_ADDR 0x4000

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

static void vdpStatus (void)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        printf ("R%d:%02X ", i, vdp.reg[i]);
    }
    printf ("\nSt:%02X\n", vdp.st);
}

BYTE vdpReadData (void)
{
    if (vdp.addr >= MAX_ADDR)
    {
        halt ("VDP read out of range");
    }

    vdp.cmdInProg = 0;
    // printf ("VDP : [%04X] -> %02X\n", vdp.addr, vdp.ram[vdp.addr]);
    return vdp.ram[vdp.addr++];
}

BYTE vdpReadStatus (void)
{
    // printf ("VDP : status -> %02X\n", vdp.st);
    vdp.cmdInProg = 0;
    return vdp.st;
}

void vdpWriteData (BYTE data)
{
    int i;

    if (vdp.addr >= MAX_ADDR)
    {
        halt ("VDP write out of range");
    }

    vdp.cmdInProg = 0;

    if (vdp.addr > 0x3FFF)
    {
        halt ("VDP memory out of range");
    }

    printf ("VDP : %02X -> [%04X] ", data, vdp.addr);

    vdp.ram[vdp.addr++] = data;

    for (i = 0; i < 8; i++)
    {
        printf ("%s", (data & 0x80) ? "*" : " ");
        data <<= 1;
    }
    printf ("\n");
}

void vdpWriteCommand (BYTE data)
{
    if (vdp.cmdInProg)
    {
        // printf ("\n** VDP **  command -> %02X/%02X\n", vdp.cmd, data);

        switch (data >> 6)
        {
        case 0:
            vdp.mode = VDP_READ;
            vdp.addr = ((data & 0x3F) << 8) | vdp.cmd;
            // printf ("\n** VDP **  read addr -> %04X\n", vdp.addr);
            break;

        case 1:
            vdp.mode = VDP_WRITE;
            vdp.addr = ((data & 0x3F) << 8) | vdp.cmd;
            // printf ("\n** VDP **  write addr -> %04X\n", vdp.addr);
            break;

        case 2:
            vdp.mode = 0;
            vdp.reg[data&7] = vdp.cmd;
            vdpStatus ();
            break;
        }
    }
    else
    {
        vdp.cmd = data;
    }

    vdp.cmdInProg = !vdp.cmdInProg;
}

#ifndef _WIN32

static void setrgbpalette (int c, int r, int g, int b)
{
    union REGS regs;

    regs.h.ah = 0x10;
    regs.h.al = 0x10;
    regs.x.bx = c;
    regs.h.dh = r;
    regs.h.ch = g;
    regs.h.cl = b;
    int86 (0x10, &regs, &regs);
}

#endif

void vdpInitGraphics (void)
{
#ifndef _WIN32
    union REGS regs;

    printf ("VDP : init graphics\n");
    // return;

    vdp.graphics = 1;
    regs.h.ah = 0;
    regs.h.al = 0x13;
    int86 (0x10, &regs, &regs);

    setrgbpalette (0x10, 0x00, 0x00, 0x00);
    setrgbpalette (0x11, 0x00, 0x00, 0x00);
    setrgbpalette (0x12, 0x00, 0x30, 0x00);
    setrgbpalette (0x13, 0x10, 0x3f, 0x10);
    setrgbpalette (0x14, 0x00, 0x00, 0x20);
    setrgbpalette (0x15, 0x10, 0x10, 0x3F);
    setrgbpalette (0x16, 0x20, 0x00, 0x00);
    setrgbpalette (0x17, 0x00, 0x3f, 0x3f);
    setrgbpalette (0x18, 0x30, 0x00, 0x00);
    setrgbpalette (0x19, 0x3f, 0x10, 0x10);
    setrgbpalette (0x1A, 0x20, 0x20, 0x00);
    setrgbpalette (0x1B, 0x3f, 0x3f, 0x10);
    setrgbpalette (0x1C, 0x00, 0x20, 0x00);
    setrgbpalette (0x1D, 0x3f, 0x10, 0x3f);
    setrgbpalette (0x1E, 0x30, 0x30, 0x30);
    setrgbpalette (0x1F, 0x3f, 0x3f, 0x3f);
#endif
}

static void vdpPlot (int x, int y, int col)
{
    if (!vdp.graphics)
        return;

#ifndef _WIN32
    pokeb (0xA000, (y * 320) + x, col + 16);
#endif
}

static void vdpDrawChar (int cx, int cy, int ch)
{
    int x, y;
    int data;
    int charpat = VDP_CHARPAT_TAB + (ch << 3);
    int col = vdp.ram[VDP_GR_COLTAB_ADDR + (ch >> 3)];

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

    // far char *addr = MK_FP (0xA000, y * 320 + x);
    //
    // switch (data >> 4)
    // {
    //     case 0: *addr++ = 0;

    for (x1 = x; x1 < (x + 8); x1++)
    {
        // printf ("SPLOT : %d,%d (col %d) \n", x1, y1, col);

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
        // printf ("Draw byte y=%d\n", y1);

        // if (count++ > 256)
        // {
        //     halt ("Sprites taking too long\n");
        // }

        vdpDrawByte (vdp.ram[p+y1], x, y + y1, c & 0x0F);

        // printf ("Drawn byte y=%d\n", y1);
    }
}

static void vdpDrawSprites16x16 (int x, int y, int p, int c)
{
    // int pat = VDP_SPRITEPAT_TAB + p * 8; // 32;
    int x1, y1, col;
    // int count = 0;

    printf ("Draw sprite pat %d at %d,%d\n", p, x, y);

    for (col = 0; col < 16; col += 8)
    {
        for (y1 = 0; y1 < 16; y1++)
        {
            // printf ("Draw byte y=%d\n", y1);

            // if (count++ > 256)
            // {
            //     halt ("Sprites taking too long\n");
            // }

            vdpDrawByte (vdp.ram[p+y1+col*2], x + col, y + y1, c & 0x0F);

            // printf ("Drawn byte y=%d\n", y1);
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
        // printf (">> i is %d\n", i);
        y = vdp.ram[attr + i*4] + 1;
        x = vdp.ram[attr + i*4 + 1];
        p = vdp.ram[attr + i*4 + 2] * 8 + VDP_SPRITEPAT_TAB;
        c = vdp.ram[attr + i*4 + 3];

        if (y == 0xD0)
        {
            // printf ("Sprite %d switched off\n", y);
            return;
        }

        // printf ("Draw sprite %d @ %d,%d pat=%d, colour=%d\n", i, x, y, p, c);

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
    static int count;
    static unsigned long lastTime;
    // static int count;
    // time_t t;
    unsigned long t;
    int sc;

    // t = time(NULL);

    t = *(long*)0x46C;

    if (!force && t == lastTime)
        return;

    lastTime = t;

    count++;

    if (count < 6)
        return;

    count = 0;

    // gromIntegrity();
    // printf ("VDP : refresh %d start\n", count);

    // printf ("VDP : refresh %d check mode\n", count);

    if (VDP_BITMAP_MODE || VDP_TEXT_MODE || VDP_MULTI_MODE)
    {
        halt ("unsupported VDP mode");
    }

    // printf ("VDP : refresh %d draw sc\n", count);

    for (sc = 0; sc < 0x300; sc++)
    {
        vdpDrawChar (sc % 32, sc / 32, vdp.ram[VDP_SCRN_IMGTAB + sc]);
    }

    vdpDrawSprites ();

    // printf ("VDP : refresh %d done\n", count++);
    // gromIntegrity();
}

