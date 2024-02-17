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

/*
 *  Dump the contents of a disk file
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "types.h"
#include "parse.h"
#include "files.h"
// #include "decodebasic.h"
#include "diskdata.h"

#define BYTES_PER_SECTOR        256

static FILE *diskFp;
static bool showContents = false;
static bool showBasic = false;

static void printVolumeHeader (FILE *disk, FILE *out);
{
    DiskVolumeHeader header;

    fseek (disk, 0, SEEK_SET);
    fread (&header, sizeof (header), 1, disk);

    fprintf (out,"Vol-Label='%-10.10s'", header.name);
    fprintf (out,", sectors=%d", ntohs (header.sectors));
    fprintf (out,", sec/trk:%d", header.secPerTrk);
    fprintf (out,", DSR:'%-3.3s'", header.dsk);
    fprintf (out,", Protect:'%c'", header.protected);
    fprintf (out,", tracks:%d", header.tracks);
    fprintf (out,", sides:%d", header.sides);
    fprintf (out,", density:%02X", header.density);

    if (header.date[0])
        fprintf (out,", year:%-8.8s", header.date);

    fprintf (out,", FAT sectors used");

    #if 1
    int max = 0x64;
    if (header.sides==2)
    {
        if (header.density == 1)
            max = 0x91;
        else
            max = 0xeb;
    }

    bool inuse = false;
    for (int i = 0; i <= max - 0x38; i++)
    {
        uint8_t map=header.bitmap[i];

        for (int j = 0; j < 8; j++)
        {
            bool bit = (map & (1<<j));

            if (!inuse && bit)
            {
                fprintf (out,"[%d-", i*8+j);
                inuse = true;
            }
            else if (inuse && !bit)
            {
                fprintf (out,"%d]", i*8+j-1);
                inuse = false;
            }
        }
    }
    if (inuse) fprintf (out,"%d]",(max-0x38+1)*8);
    #endif
    printf ("\n");
}

void diskDumpContents (int sectorStart, int sectorCount, int recLen)
{
    if (!showContents)
        return;

    for (int i = sectorStart; i <= sectorStart+sectorCount; i++)
    {
        int8_t data[BYTES_PER_SECTOR];
        fseek (diskFp, BYTES_PER_SECTOR * i, SEEK_SET);

        fread (&data, sizeof (data), 1, diskFp);

        for (int j = 0; j < BYTES_PER_SECTOR; j += recLen)
        {
            printf ("\n\t'");
            for (int k = 0; k < recLen; k++)
            {
                printf ("%c", isalnum (data[j+k]) ? data[j+k] : '.');
            }
            printf ("'");
        }
    }
}

static printFileInfo (FILE *out, DiskFileInfo *header)
{
    fprintf (out, "%-10.10s", header->name);
    fprintf (out, " %6d", header->sector);

    if (header->flags & 0x01)
    {
        length = (ntohs (header->alloc) - 1) * BYTES_PER_SECTOR + header->eof;
        if (showBasic)
            prog = malloc (ntohs (header->alloc) * BYTES_PER_SECTOR);
    }
    else
        length = ntohs(header->len);

    fprintf (out, " %6d", length);

    fprintf (out, " %02X%-17.17s", header->flags,filesShowFlags (header->flags));
    fprintf (out, " %8d", ntohs(header->alloc));
    fprintf (out, " %8d", header->recSec * ntohs (header->alloc));
    fprintf (out, " %10d", header->eof);
    fprintf (out, " %7d", ntohs(header->l3Alloc));
    fprintf (out, " %7d", header->recLen);

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=header->chain[i];
        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeChain (chain, &start, &len);
            fprintf (out, "%s%2d=(%d-%d)", i!=0?",":"", i, start, start+len);

            if (prog)
            {
                progBytes = readProg (prog, progBytes, start, len);
            }
            else
                dumpContents (start, len, header->recLen);
        }
    }

    if (prog)
        decodeBasicProgram (prog, length);

    fprintf (out, "\n");
}

static void printDirectory (FILE *disk, FILE *out, int sector)
{
    DiskFileInfo headers[MAX_FILE_COUNT];
    int count = diskAnalyseDirectory (disk, sector, headers);

    if (count < 0)
        return count;

    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sectors\n");
    fprintf (out, "========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");

    for (int i = 0; i < count; i++)
    {
        printfFileInfo (out, headers[i]);
    }
}


int main (int argc, char *argv[])
{
    char c;

    while ((c = getopt(argc, argv, "db")) != -1)
    {
        switch (c)
        {
            case 'd' : showContents = true; break;
            case 'b' : showBasic = true; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nSector dump disk file read tool\n\n"
                "usage: %s [-d] [-b] <dsk-file>\n"
                "\t where -d=dump HEX, -b=decode basic\n\n", argv[0]);
        return 1;
    }

    if ((diskFp = fopen (argv[optind], "r")) == NULL)
    {
        printf ("Can't open %s\n", argv[1]);
        return 0;
    }

    printVolumeHeader (diskFp, stdout);
    printDirectory (diskFp, 1);

    fclose (diskFp);

    return 0;
}
