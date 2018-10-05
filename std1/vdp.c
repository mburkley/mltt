
#include "vdp.h"

#define VDP_READ 1
#define VDP_WRITE 2

struct
{
    WORD addr;
    int cmdInProg;
    int mode;
    BYTE reg[8];
    BYTE cmd;
    BYTE st;
    BYTE ram[16384];
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
    printf ("\n** VDP **  [%04X] -> %02X\n", vdp.addr, vdp.ram[vdp.addr]);
    return vdp.ram[vdp.addr++];
}

BYTE vdpReadStatus (void)
{
    printf ("\n** VDP **  status -> %02X\n", vdp.st);
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

    printf ("\n** VDP **  %02X -> [%04X]\n", data, vdp.addr);
    vdp.ram[vdp.addr++] = data;
}

void vdpWriteCommand (BYTE data)
{
    if (vdp.cmdInProg)
    {
        printf ("\n** VDP **  command -> %02X/%02X\n", vdp.cmd, data);

        switch (data >> 6)
        {
        case 0:
            vdp.mode = VDP_READ;
            vdp.addr = data & 0x3F << 8 | vdp.cmd;
            break;

        case 1:
            vdp.mode = VDP_WRITE;
            vdp.addr = data & 0x3F << 8 | vdp.cmd;
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

