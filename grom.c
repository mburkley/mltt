#include <stdio.h>
#include <stdlib.h>

#include "grom.h"
#include "trace.h"
#include "gpl.h"

struct
{
    WORD addr;
    BYTE lowByteGet;
    BYTE lowByteSet;

    union
    {
        // WORD w[0x4000];
        BYTE b[0x8000];
    }
    rom;
}
gRom;

void gromIntegrity (void)
{
    if (gRom.rom.b[0x1bc] != 0xbe)
    {
        halt ("GROM corruption\n");
    }
}

int gromRead (int addr, int size)
{
    #if 0
    if (size != 1)
    {
        halt ("GROM can only read bytes\n");
    }
    #endif

    switch (addr)
    {
    case 0:
        gRom.lowByteGet = 0;
        gRom.lowByteSet = 0;

        BYTE result;

        if (gRom.addr > 0x7FFF)
        {
            result = 0;
        }
        else
        {
            result = gRom.rom.b[gRom.addr];
        }

        mprintf (LVL_GROM, "GROMRead: %04X : %02X\n",
                 (unsigned) gRom.addr,
                 (unsigned) result);

        gplDisassemble (gRom.addr, result);
        gRom.addr++;

        return result;
    case 2:
        if (gRom.lowByteGet)
        {
            gRom.lowByteGet = 0;
            mprintf (LVL_GROM, "GROMAD addr get as %04X\n", gRom.addr+1);
            return (gRom.addr+1) & 0xFF;
        }

        gRom.lowByteGet = 1;
        mprintf (LVL_GROM, "GROMAD lo byte Get\n");
        return (gRom.addr+1) >> 8;
    default:
        halt ("Strange GROM CPU addr\n");
        break;
    }

    return 0;
}

void gromWrite (int addr, int data, int size)
{
    if (size != 1)
    {
        halt ("GROM can only read bytes\n");
    }

    switch (addr)
    {
    case 2:
        mprintf (LVL_GROM, "GROMAD BYTE write to 9C02\n");
        if (gRom.lowByteSet)
        {
            gRom.addr = (gRom.addr & 0xFF00) | data;
            gRom.lowByteSet = 0;
            gRom.lowByteGet = 0;
            mprintf (LVL_GROM, "GROMAD addr set to %04X\n", gRom.addr);
        }
        else
        {
            gRom.addr = data << 8;
            gRom.lowByteSet = 1;
            mprintf (LVL_GROM, "GROMAD lo byte Set to %x\n", data);
        }
        break;
    default:
        printf ("%s addr=%x data=%x\n", __func__, addr, data);
        // halt ("invalid GROM write operation");
        printf ("GROM write data ignored\n");
        break;
    }
}

void gromShowStatus (void)
{
    mprintf (LVL_GROM, "GROM\n");
    mprintf (LVL_GROM, "====\n");

    mprintf (LVL_GROM, "addr        : %04X\n", gRom.addr);
    mprintf (LVL_GROM, "half-ad-set : %d\n", gRom.lowByteSet);
    mprintf (LVL_GROM, "half-ad-get : %d\n", gRom.lowByteGet);
}

void gromLoad (char *name, int start, int len)
{
    FILE *fp;

    printf("%s %s %x %x\n", __func__, name, start, len);

    if ((fp = fopen (name, "rb")) == NULL)
    {
        printf ("can't open %s\n", name);
        exit (1);
    }

    if (fread (gRom.rom.b + start, sizeof (BYTE), len, fp) != len)
    {
        halt ("GROM file read failure");
    }

    fclose (fp);

    printf ("GROM load ok\n");
}

