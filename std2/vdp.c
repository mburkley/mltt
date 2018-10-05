#include <stdio.h>
#include <time.h>
#include <dos.h>

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

struct
{
    WORD addr;
    int cmdInProg;
    int mode;
    BYTE reg[8];
    BYTE cmd;
    BYTE st;
    BYTE ram[16384];
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
    vdp.cmdInProg = 0;
    // printf ("VDP : [%04X] -> %02X\n", vdp.addr, vdp.ram[vdp.addr]);
    return vdp.ram[vdp.addr++];
}

BYTE vdpReadStatus (void)
{
    printf ("VDP : status -> %02X\n", vdp.st);
    vdp.cmdInProg = 0;
    return vdp.st;
}

void vdpWriteData (BYTE data)
{
    vdp.cmdInProg = 0;

    if (vdp.addr > 0x3FFF)
    {
        halt ("VDP memory out of range");
    }

    // printf ("VDP : %02X -> [%04X]\n", data, vdp.addr);
    vdp.ram[vdp.addr++] = data;
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

void vdpInitGraphics (void)
{
    union REGS regs;

    printf ("VDP : init graphics\n");
    // return;

    vdp.graphics = 1;
    regs.h.ah = 0;
    regs.h.al = 0x13;
    int86 (0x10, &regs, &regs);

}

static void vdpPlot (int x, int y, int col)
{
    pokeb (0xA000, (y * 320) + x, col + 7);
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

void vdpRefresh (int force)
{
    static time_t lastTime;
    static int count;
    time_t t;
    int sc;

    t = time(NULL);

    if (!force && t <= lastTime)
        return;

    lastTime = t;

    printf ("VDP : refresh %d\n", count++);

    if (!vdp.graphics)
        return;

    if (VDP_BITMAP_MODE || VDP_TEXT_MODE || VDP_MULTI_MODE)
    {
        halt ("unsupported VDP mode");
    }

    for (sc = 0; sc < 0x300; sc++)
    {
        vdpDrawChar (sc % 32, sc / 32, vdp.ram[VDP_SCRN_IMGTAB + sc]);
    }

}

