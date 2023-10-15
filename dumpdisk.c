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

/*
 *  Dump the contents of a disk file
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <arpa/inet.h>

FILE *diskFp;

static struct
{
    char name[10];
    int16_t len;
    uint8_t flags;
    uint8_t recSec;
    int16_t alloc;
    uint8_t eof;
    uint8_t recLen;
    int16_t l3Alloc;
    char dtCreate[4];
    char dtUpdate[4];
    uint8_t chain[76][3];
}
fileHeader;

static struct
{
    char name[10];
    int16_t sectors;
    uint8_t secPerTrk;
    char dsk[3];
    // 0x10
    uint8_t protected; // 'P' = protected, ' '=not
    uint8_t tracks;
    uint8_t sides;
    uint8_t density; // SS=1,DS=2
    uint8_t tbd3; // reserved
    uint8_t fill1[11];
    // 0x20
    uint8_t date[8];
    uint8_t fill2[16];
    // 0x38
    uint8_t bitmap[0xc8]; // bitmap 38-64, 38-91 or 38-eb for SSSD,DSSD,DSDD respec
}
volumeHeader;

static char * showFlags (uint8_t flags)
{
    static char str[16];

    sprintf (str, "%s%s%s%s", 
        (flags & 0x80) ? "VAR":"FIX",
        (flags & 0x08) ? "-WP" : "",
        (flags & 0x02) ? "-BIN" : "-ASC",
        (flags & 0x01) ? "-PROG" : "-DATA");

    return str;
}

static void decodeChain (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

static void analyseFirstSector (void)
{
    fseek (diskFp, 0, SEEK_SET);

    /*  Can't find documentatoin for first sector, decoding from examples */
    fread (&volumeHeader, sizeof (volumeHeader), 1, diskFp);
    printf ("Vol-Label='%-10.10s'", volumeHeader.name);
    printf (", sectors=%d", ntohs (volumeHeader.sectors));
    printf (", sec/trk:%d", volumeHeader.secPerTrk);
    printf (", DSR:'%-3.3s'", volumeHeader.dsk);
    printf (", Protect:'%c'", volumeHeader.protected);
    printf (", tracks:%d", volumeHeader.tracks);
    printf (", sides:%d", volumeHeader.sides);
    printf (", density:%02X", volumeHeader.density);
    printf (", year:%-8.8s", volumeHeader.date);

    printf (", Chains");

    int max = 0x64;
    if (volumeHeader.sides==2)
    {
        if (volumeHeader.density == 2)
            max = 0x91;
        else
            max = 0xeb;
    }
    for (int i = 0; i < max - 0x38; i+=2)
    {
        uint8_t *chain=&volumeHeader.bitmap[i];
        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeChain (chain, &start, &len);
            printf (",%2d=(%d-%d)", i, start, start+len);
        }
    }
    printf ("\n");
}

static void analyse (int sector)
{
    printf ("SEC-%3d:", sector);
    fseek (diskFp, 256 * sector, SEEK_SET);

    fread (&fileHeader, sizeof (fileHeader), 1, diskFp);
    printf ("name='%-10.10s'", fileHeader.name);
    printf (", len=%d", ntohs(fileHeader.len));
    printf (", flags=%02X(%s)", fileHeader.flags,showFlags (fileHeader.flags));
    printf (", rec/sec=%02X", fileHeader.recSec);
    printf (", sec alloc= %02X", ntohs(fileHeader.alloc));
    printf (", eof off=%02X", fileHeader.eof);
    printf (", l3alloc=%d", ntohs(fileHeader.l3Alloc));
    printf (", log rec len= %d", fileHeader.recLen);

    printf (", Chains");

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=fileHeader.chain[i];
        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeChain (chain, &start, &len);
            printf (",%2d=(%d-%d)", i, start, start+len);
        }
    }
    printf ("\n");
}

static void analyseDirectory (int sector)
{
    uint8_t data[128][2];

    fseek (diskFp, 256 * sector, SEEK_SET);

    fread (&data, sizeof (data), 1, diskFp);

    for (int i = 0; i < 128; i++)
    {
        sector = data[i][0] << 8 | data[i][1];
        if (sector == 0)
            break;
        analyse (sector);
    }
}

int main (int argc, char *argv[])
{
    const char *name;

    if (argc < 2)
        name = "disk1.dsk";
    else
        name = argv[1];

    if ((diskFp = fopen (name, "r")) == NULL)
    {
        printf ("Can't open %s\n", name);
        return 0;
    }

    analyseFirstSector ();
    analyseDirectory (1);
    #if 0
    // analyse (0);
     analyse (2);
     analyse (2);
     analyse (3);
     analyse (4);
     analyse (7);
     analyse (8);
     analyse (0x1d);
     analyse (0x1e);
     analyse (0x26);
     analyse (0x28);
     #endif

    fclose (diskFp);

    return 0;
}
