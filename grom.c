#include <stdio.h>
#include <stdlib.h>

#include "grom.h"
#include "trace.h"

#define DBG_LVL 1

#define GROM_DEBUG 1

struct
{
    WORD addr;
    BYTE lowByteGet;
    BYTE lowByteSet;

    union
    {
        // WORD w[32768];
        // BYTE b[65536];
        WORD w[0x4000];
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

BYTE gromRead (void)
{
    BYTE result;

    // gromIntegrity();
    gRom.lowByteGet = 0;
    gRom.lowByteSet = 0;

    if (gRom.addr > 0x7FFF)
    {
        result = 0;
    }
    else
    {
        result = gRom.rom.b[gRom.addr];
    }

#ifdef __GROM_DEBUG
    mprintf (LVL_GROM, "GROMRead: %04X : %02X\n",
             (unsigned) gRom.addr,
             (unsigned) result);
#endif

    gRom.addr++;

    return result;
}

void gromSetAddr (WORD addr)
{
    if (gRom.lowByteSet)
    {
        gRom.addr = gRom.addr & 0xFF00 | addr;
        gRom.lowByteSet = 0;
        gRom.lowByteGet = 0;
#ifdef __GROM_DEBUG
        mprintf (LVL_GROM, "GROMAD addr set to %04X\n", gRom.addr);
#endif
    }
    else
    {
        gRom.addr = addr << 8;
        gRom.lowByteSet = 1;
#ifdef __GROM_DEBUG
        mprintf (LVL_GROM, "GROMAD lo byte Set\n");
#endif
    }
}

BYTE gromGetAddr (void)
{
    if (gRom.lowByteGet)
    {
        gRom.lowByteGet = 0;
#ifdef __GROM_DEBUG
        mprintf (LVL_GROM, "GROMAD addr get as %04X\n", gRom.addr);
#endif
        return (gRom.addr) & 0xFF;
    }

    gRom.lowByteGet = 1;
#ifdef __GROM_DEBUG
    mprintf (LVL_GROM, "GROMAD lo byte Get\n");
#endif
    return (gRom.addr) >> 8;
}

void showGromStatus (void)
{
    mprintf (LVL_GROM, "GROM\n");
    mprintf (LVL_GROM, "====\n");

    mprintf (LVL_GROM, "addr        : %04X\n", gRom.addr);
    mprintf (LVL_GROM, "half-ad-set : %d\n", gRom.lowByteSet);
    mprintf (LVL_GROM, "half-ad-get : %d\n", gRom.lowByteGet);
}

void loadGRom (void)
{
    FILE *fp;

    if ((fp = fopen ("../994agrom.bin", "rb")) == NULL)
    {
        printf ("can't open grom.hex\n");
        exit (1);
    }

    if (fread (gRom.rom.b, sizeof (BYTE), 0x6000, fp) != 0x6000)
    {
        halt ("GROM file read failure");
    }

    fclose (fp);

    if ((fp = fopen ("../module-g.bin", "rb")) == NULL)
    {
        printf ("can't open munchmng\n");
        exit (1);
    }

    if (fread (gRom.rom.b + 0x6000, sizeof (BYTE), 0x2000, fp) != 0x2000)
    {
        halt ("GROM file read failure");
    }

    fclose (fp);
    printf ("GROM load ok\n");
}


