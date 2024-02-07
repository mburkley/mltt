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
 *  Provides disk data structure operation functions
 */

#include <string.h>
#include <stdio.h>

#include "diskdata.h"

void diskDecodeChain (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

void diskEncodeChain (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

void diskDecodeVolumeHeader (uint8_t sector[])
{
    DISKVOLHDR *v = (DISKVOLHDR) sector;

    fseek (diskFp, 0, SEEK_SET);

    // fread (&volumeHeader, sizeof (volumeHeader), 1, diskFp);
    printf ("Vol-Label='%-10.10s'", v->name);
    printf (", sectors=%d", ntohs (v->sectors));
    printf (", sec/trk:%d", v->secPerTrk);
    printf (", DSR:'%-3.3s'", v->dsk);
    printf (", Protect:'%c'", v->protected);
    printf (", tracks:%d", v->tracks);
    printf (", sides:%d", v->sides);
    printf (", density:%02X", v->density);

    if (v->date[0])
        printf (", year:%-8.8s", v->date);

    printf (", FAT sectors used");

    #if 1
    int max = 0x64;
    if (v->sides==2)
    {
        if (v->density == 1)
            max = 0x91;
        else
            max = 0xeb;
    }

    bool inuse = false;
    for (int i = 0; i <= max - 0x38; i++)
    {
        uint8_t map=v->bitmap[i];

        for (int j = 0; j < 8; j++)
        {
            bool bit = (map & (1<<j));

            if (!inuse && bit)
            {
                printf ("[%d-", i*8+j);
                inuse = true;
            }
            else if (inuse && !bit)
            {
                printf ("%d]", i*8+j-1);
                inuse = false;
            }
        }
    }
    if (inuse) printf ("%d]",(max-0x38+1)*8);
    #endif
    printf ("\n");
}

void diskAllocBitMap (uint8_t sector[], int start, int count)
{
    volumeHeader *v = (volumeHeader*) sector;

    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        v->bitmap[byte] |= (1<<bit);
    }
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

void diskEncodeVolumeHeader (uint8_t sector[], const char *name)
{
    volumeHeader *v = (volumeHeader*) sector;
    strncpy (v->name, name, 10);
    v->sectors = htons (SECTORS_PER_DISK);
    v->secPerTrk = 9;
    memcpy (v->dsk, "DSK", 3);
    v->protected = ' ';
    v->tracks = 40;
    v->sides = 2;
    v->density = 1;
}

void diskAnalyseFile (int sector)
{
    int length;
    uint8_t *prog = NULL;
    int progBytes = 0;

    fseek (diskFp, BYTES_PER_SECTOR * sector, SEEK_SET);

    fread (&fileHeader, sizeof (fileHeader), 1, diskFp);
    printf ("%-10.10s", fileHeader.name);

    printf (" %6d", sector);

    if (fileHeader.flags & 0x01)
    {
        length = (ntohs (fileHeader.alloc) - 1) * BYTES_PER_SECTOR + fileHeader.eof;
        if (showBasic)
            prog = malloc (ntohs (fileHeader.alloc) * BYTES_PER_SECTOR);
    }
    else
        length = ntohs(fileHeader.len);

    printf (" %6d", length);

    printf (" %02X%-17.17s", fileHeader.flags,filesShowFlags (fileHeader.flags));
    printf (" %8d", ntohs(fileHeader.alloc));
    printf (" %8d", fileHeader.recSec * ntohs (fileHeader.alloc));
    printf (" %10d", fileHeader.eof);
    printf (" %7d", ntohs(fileHeader.l3Alloc));
    printf (" %7d", fileHeader.recLen);

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=fileHeader.chain[i];
        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeChain (chain, &start, &len);
            printf ("%s%2d=(%d-%d)", i!=0?",":"", i, start, start+len);

            if (prog)
            {
                progBytes = readProg (prog, progBytes, start, len);
            }
            else
                dumpContents (start, len, fileHeader.recLen);
        }
    }

    if (prog)
        decodeBasicProgram (prog, length);

    printf ("\n");
}

void diskAnalyseDirectory (FILE *fp, int sector)
{
    uint8_t data[BYTES_PER_SECTOR/2][2];

    fseek (fp, BYTES_PER_SECTOR * sector, SEEK_SET);
    fread (&data, sizeof (data), 1, fp);

    printf ("Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sectors\n");
    printf ("========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");
    for (int i = 0; i < BYTES_PER_SECTOR/2; i++)
    {
        sector = data[i][0] << 8 | data[i][1];
        if (sector == 0)
            break;
        diskAnalyseFile (sector);
    }
}

int diskReadData (uint8_t *buff, int offset, int sectorStart, int sectorCount)
{
    for (int i = sectorStart; i <= sectorStart+sectorCount; i++)
    {
        fseek (diskFp, BYTES_PER_SECTOR * i, SEEK_SET);

        fread (&buff[offset], BYTES_PER_SECTOR, 1, diskFp);
        offset += BYTES_PER_SECTOR;
    }

    return offset;
}

