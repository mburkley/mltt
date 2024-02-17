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

void diskAnalyseFile (FILE *fp, int sector)
{
    int length;
    uint8_t *prog = NULL;
    int progBytes = 0;

    fseek (fp, BYTES_PER_SECTOR * sector, SEEK_SET);
    fread (&fileHeader, sizeof (fileHeader), 1, fp);
}

/*  Returns number of files */
int diskAnalyseDirectory (FILE *fp, int sector, DiskFileHeader *headers)
{
    uint8_t data[BYTES_PER_SECTOR/2][2];

    fseek (fp, BYTES_PER_SECTOR * sector, SEEK_SET);
    fread (&data, sizeof (data), 1, fp);

    for (int i = 0; i < BYTES_PER_SECTOR/2; i++)
    {
        sector = data[i][0] << 8 | data[i][1];
        if (sector == 0)
            break;
        diskAnalyseFile (sector, headers[i]);
    }

    return i;
}

// int diskReadData (uint8_t *buff, int offset, int sectorStart, int sectorCount)
int diskReadData (DiskFileInfo *info, unsigned char *buff, int offset, int len)
{
    for (int i = 0; i < chainCount; i++)
    {
        fseek (diskFp, BYTES_PER_SECTOR * i, SEEK_SET);

        fread (&buff[offset], BYTES_PER_SECTOR, 1, diskFp);
        offset += BYTES_PER_SECTOR;
    }

    return offset;
}

