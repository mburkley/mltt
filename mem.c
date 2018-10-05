#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "mem.h"
#include "grom.h"
#include "vdp.h"

#define DBG_LVL 3

typedef union
{
    // WORD w[32768];
    // BYTE b[65536];
    WORD w[0x1000];
    BYTE b[0x2000];
}
memPage;

typedef struct
{
    BOOL    rom;
    BOOL    mapped;
    memPage* m;
}
memMap;

memPage page[5];

union
{
    WORD w[0x80];
    BYTE b[0x100];
}
scratch;

memMap map[8] =
{
    { 1, 0, &page[0] }, // Console ROM
    { 0, 0, &page[1] }, // 32k Expn low
    { 1, 0, NULL     }, // Device ROM (selected by CRU)
    { 1, 0, &page[2] }, // Cartridge ROM
    { 0, 1, NULL     }, // Memory mapped devices
    { 0, 0, &page[3] }, // +
    { 0, 0, &page[1] }, // + 32k expn high
    { 0, 0, &page[1] }  // +
};

static WORD memMappedRead (WORD addr, BOOL byte)
{
    switch (addr & 0xFC00)
    {
    case 0x8000:
        if (byte)
        {
#ifdef __MEM_DEBUG
            mprintf (DBG_LVL, "[ScratchRB:%04X->%04X->%02X]", addr & 0xFF,
                     scratch.w[(addr & 0xFF) >> 1],
                     scratch.b[(addr ^ 1) & 0xFF]);
#endif

            return scratch.b[(addr ^ 1) & 0xFF];
        }
        else
        {
#ifdef __MEM_DEBUG
            mprintf (DBG_LVL, "[ScratchRW:%04X->%02X]", addr & 0xFF,
                     scratch.w[(addr & 0xFF) >> 1]);
#endif

            return scratch.w[(addr & 0xFF) >> 1];
        }

        // break;

    case 0x9800:
        /*
         *  Console GROMs respond to any base
         */

        if ((addr & 0x03) == 0x00)
        {
            // tms9900.ram->b[addr ^ 1] = gromRead ();
            return gromRead();
        }
        else if ((addr & 0x03) == 0x02)
        {
            // tms9900.ram->b[addr ^ 1] = gromGetAddr ();
            return gromGetAddr ();
        }
        else
        {
            halt ("Strange GROM CPU addr\n");
        }
        break;

    case 0x8800:
        if (addr == 0x8800)
        {
            // tms9900.ram->b[0x8801] = vdpReadData ();
            return vdpReadData ();
        }
        else if (addr == 0x8802)
        {
            // tms9900.ram->b[0x8803] =
            return vdpReadStatus ();
        }
        break;
    }

    return 0;
}

static void memMappedWrite (WORD addr, WORD data, BOOL byte)
{
    switch (addr & 0xFC00)
    {
    case 0x8000:
        if (byte)
        {
#ifdef __MEM_DEBUG
            mprintf (DBG_LVL, "[ScratchWB:%04X<-%02X]", addr & 0xFF, data);
#endif

            scratch.b[(addr ^ 1) & 0xFF] = data;
        }
        else
        {
#ifdef __MEM_DEBUG
            mprintf (DBG_LVL, "[ScratchWW:%04X<-%02X]", addr & 0xFF, data);
#endif

            scratch.w[(addr & 0xFF) >> 1] = data;
        }

        break;

    case 0x9C00:
        if ((addr & 0x03) == 0x00)
        {
            halt ("Invalid byte write to GROM addr");
            gromSetAddr (data);
        }
        else if ((addr & 0x03) == 0x02)
        {
            mprintf (DBG_LVL, "GROMAD BYTE write to 9802\n");
            gromSetAddr (data);
        }
        else
        {
            halt ("invalid GROM write operation");
            return;
        }
        break;

    case 0x8C00:
        if (addr == 0x8C00)
        {
            vdpWriteData (data);
        }
        else if (addr == 0x8C02)
        {
            vdpWriteCommand (data);
        }
        else
        {
            printf ("attempt to write %02X to %04X\n", data, addr);
            halt ("invalid VDP write operation");
        }
        break;
    }
}

WORD memRead(WORD addr)
{
    memMap *p = &map[addr>>13];

    if (p->mapped)
    {
        return memMappedRead (addr, 0);
    }

    if (!p->m)
    {
        /*
         *  Not installed
         */

        return 0;
    }


#ifdef __MEM_DEBUG
    mprintf (DBG_LVL, "[memRead:%04X->%04X]", addr, p->m->w[(addr&0x1FFF)>>1]);
#endif

    // tms9900.ram->w[addr>>1];
    return p->m->w[(addr&0x1FFF)>>1];
}

void memWrite(WORD addr, WORD data)
{
    memMap *p = &map[addr>>13];

    if (p->mapped)
    {
        memMappedWrite (addr, data, 0);
        return;
    }

    if (p->rom)
    {
        printf ("addr=%04X\n", addr);
        halt ("Attempted write to ROM");
    }

    if (!p->m)
    {
        halt ("Attempted write WORD to uninstalled memory\n");
    }

#ifdef __MEM_DEBUG
    mprintf (DBG_LVL, "[memWrite:%04X<-%04X]", addr, p->m->w[(addr&0x1FFF)>>1]);
#endif

    // tms9900.ram->w[addr>>1] = data;
    p->m->w[(addr&0x1FFF)>>1] = data;
}

WORD memReadB(WORD addr)
{
    memMap *p = &map[addr>>13];

    if (p->mapped)
    {
        return memMappedRead (addr, 1);
    }

    if (!p->m)
    {
        return 0;
    }

    addr = addr ^ 1;


#ifdef __MEM_DEBUG
    mprintf (DBG_LVL, "[memReadB:%04X->%02X]", addr, p->m->b[addr&0x1FFF]);
#endif

    // return tms9900.ram->b[addr];
    return p->m->b[addr&0x1FFF];
}

void memWriteB(WORD addr, BYTE data)
{
    memMap *p = &map[addr>>13];

    if (p->mapped)
    {
        memMappedWrite (addr, data, 1);
        return;
    }

    if (p->rom)
    {
        printf ("addr=%04X\n", addr);
        halt ("Attempted write to ROM");
    }

    if (!p->m)
    {
        halt ("Attempted write to uninstalled memory\n");
    }

    addr = addr ^ 1;


#ifdef __MEM_DEBUG
    mprintf (DBG_LVL, "[memWriteB:%04X<-%02X]", addr, p->m->b[addr&0x1FFF]);
#endif

    // tms9900.ram->b[addr] = data;
    p->m->b[addr&0x1FFF] = data;
}

WORD * memWordAddr (WORD addr)
{
    memMap *p = &map[addr>>13];

    if (p->m && !p->mapped)
    {
        printf ("Address of >%04X in x86 space is %p\n",
                addr, &p->m->w[(addr&0x1fff)>>1]);

        return &p->m->w[(addr&0x1fff)>>1];
    }

    if (addr >= 0x8000 && addr <= 0x83FF)
    {
        // printf ("Address of (scratchpad) >%04X in x86 space is %p\n",
        //         addr, &scratch.w[(addr & 0xFF)>>1]);

        return &scratch.w[(addr & 0xFF)>>1];
    }

    halt ("get addr of non RAM\n");
    return NULL;
}

void memLoad (char *file, WORD addr, WORD length)
{
    FILE *fp;
    int i;
    memMap *p;

    if ((fp = fopen (file, "rb")) == NULL)
    {
        printf ("can't open ROM bin file '%s'\n", file);
        exit (1);
    }

    for (i = addr; i < addr + length; i += 0x2000)
    {
        p = &map[i>>13];

        if (fread (p->m->b, sizeof (BYTE), 0x2000, fp) != 0x2000)
        {
            halt ("ROM file read failure");
        }
    }

    fclose (fp);

    for (i = addr; i < addr + length; i += 2)
    {
        p = &map[i>>13];

        p->m->w[(i&0x1fff)>>1] = SWAP (p->m->w[(i&0x1fff)>>1]);
    }
}

#ifdef __UNIT_TEST

#include <stdarg.h>

void halt (char *s)
{
    s=s;
}

int mprintf (int level, char *s, ...)
{
    va_list ap;

    level = level;
    va_start (ap, s);

    vprintf (s, ap);

    va_end (ap);

    return 0;
}

int main (void)
{
    WORD *r0, *r8;

    r0 = memWordAddr (0x83E0);
    r8 = memWordAddr (0x83F0);

    printf ("Set R0 to AA55 and R8 to A55A\n");
    *r0 = 0xAA55;
    *r8 = 0xA55A;

    printf ("memRead(83E0)  = %04X\n", memRead (0x83E0));
    printf ("memReadB(83E0) = %04X\n", memReadB (0x83E0));
    printf ("memReadB(83E1) = %04X\n", memReadB (0x83E1));

    printf ("memRead(83F0)  = %04X\n", memRead (0x83F0));
    printf ("memReadB(83F0) = %04X\n", memReadB (0x83F0));
    printf ("memReadB(83F1) = %04X\n", memReadB (0x83F1));

    return 0;
}

#endif

