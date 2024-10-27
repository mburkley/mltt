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
#include <stdlib.h>

#include "grom.h"
#include "trace.h"
#include "gpl.h"

#define GROM_LEN        0x10000

struct
{
    uint16_t addr;
    uint8_t lowByteGet;
    uint8_t lowByteSet;
    uint8_t b[GROM_LEN];
}
gRom;

void gromIntegrity (void)
{
    if (gRom.b[0x1bc] != 0xbe)
    {
        halt ("GROM corruption\n");
    }
}

uint16_t gromRead (uint8_t *ptr, uint16_t addr, int size)
{
    switch (addr)
    {
    case 0:
        gRom.lowByteGet = false;
        gRom.lowByteSet = false;

        uint8_t result;

        result = gRom.b[gRom.addr];

        mprintf (LVL_GROM, "GROMRead: %04X : %02X\n",
                 (unsigned) gRom.addr,
                 (unsigned) result);

        //  TODO disabled for now.  GPL disassembly requires the CPU PC which is not available here
        // gplDisassemble (gRom.addr, result);
        gRom.addr++;

        return result;
    case 2:
        if (gRom.lowByteGet)
        {
            gRom.lowByteGet = false;
            mprintf (LVL_GROM, "GROMAD addr get as %04X\n", gRom.addr+1);
            return (gRom.addr+1) & 0xFF;
        }

        gRom.lowByteGet = true;
        mprintf (LVL_GROM, "GROMAD lo byte Get\n");
        return (gRom.addr+1) >> 8;
    default:
        halt ("Strange GROM CPU addr\n");
        break;
    }

    return 0;
}

void gromWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size)
{
    if (size != 1)
    {
        halt ("GROM can only read bytes\n");
    }

    switch (addr)
    {
    case 2:
        mprintf (LVL_GROM, "GROMAD uint8_t write to 9C02\n");
        if (gRom.lowByteSet)
        {
            gRom.addr = (gRom.addr & 0xFF00) | data;
            gRom.lowByteSet = false;
            gRom.lowByteGet = false;
            mprintf (LVL_GROM, "GROMAD addr set to %04X\n", gRom.addr);
        }
        else
        {
            gRom.addr = data << 8;
            gRom.lowByteSet = true;
            mprintf (LVL_GROM, "GROMAD lo byte Set to %x\n", data);
        }
        break;
    default:
        printf ("%s addr=%x data=%x\n", __func__, addr, data);
        printf ("GROM write data ignored\n");
        break;
    }
}

uint8_t gromData (int addr)
{
    return gRom.b[addr];
}

uint16_t gromAddr (void)
{
    return gRom.addr;
}

void gromShowStatus (void)
{
    mprintf (LVL_GROM, "GROM\n");
    mprintf (LVL_GROM, "====\n");

    mprintf (LVL_GROM, "addr        : %04X\n", gRom.addr);
    mprintf (LVL_GROM, "half-ad-set : %d\n", gRom.lowByteSet);
    mprintf (LVL_GROM, "half-ad-get : %d\n", gRom.lowByteGet);
}

void gromLoad (char *file, uint16_t addr)
{
    FILE *fp;

    if ((fp = fopen (file, "rb")) == NULL)
    {
        printf ("can't open %s\n", file);
        exit (1);
    }

    fseek (fp, 0, SEEK_END);
    unsigned len = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    printf("%s %s %x-%x\n", __func__, file, addr, addr+len);

    if (addr + len > GROM_LEN)
    {
        printf ("Grom end %04X\n", addr+len);
        halt ("GROM too big");
    }

    if (fread (gRom.b + addr, sizeof (uint8_t), len, fp) != len)
    {
        halt ("GROM file read failure");
    }

    fclose (fp);

    printf ("GROM load ok\n");
}

