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
 *  Provides disk data structure operation functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "files.h"
#include "dskdata.h"

static void decodeChain (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

void encodeChain (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

void dskAllocBitMap (DskInfo *info, int start, int count)
{
    DskVolumeHeader *v = &info->volhdr;

    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        v->bitmap[byte] |= (1<<bit);
    }
}

void dskEncodeVolumeHeader (DskInfo *info, const char *name)
{
    DskVolumeHeader *v = &info->volhdr;
    filesLinux2TI (name, v->tiname);
    // v->sectors = htons (SECTORS_PER_DISK);
    v->sectors = htons (720);  // TODO make this a param
    v->secPerTrk = 9;
    memcpy (v->dsk, "DSK", 3);
    v->protected = ' ';
    v->tracks = 40;
    v->sides = 2;
    v->density = 1;
}

int dskCheckFileAccess (DskInfo *info, const char *path, int flags)
{
    // TODO check flags

    for (int i = 0; i < info->fileCount; i++)
    {
        if (!strcmp (path, info->files[i].osname))
        {
            printf ("# %s %s index=%d\n", __func__, path, i);
            return i;
        }
    }

    return -1;
}

const char *dskFileName (DskInfo *info, int index)
{
    return info->files[index].osname;
}

int dskFileLength (DskInfo *info, int index)
{
    return info->files[index].length;
}

bool dskFileProtected (DskInfo *info, int index)
{
    return info->files[index].filehdr.flags & FLAG_WP;
}

int dskFileCount (DskInfo *info)
{
    return info->fileCount;
}

int dskCreateFile (DskInfo *info, const char *path, int mode)
{
    return 0;
}

#if 0
void diskAnalyseFile (FILE *fp, int sector, DiskFileHeader *header)
{
    int length;
    uint8_t *prog = NULL;
    int progBytes = 0;

}
#endif

static void decodeFileChains (DskFileInfo *file)
{
    // TODO why 23?
    file->chainCount = 0;

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=file->filehdr.chain[i];
        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeChain (chain, &start, &len);

            file->chains[file->chainCount].start = start;
            file->chains[file->chainCount].end = start+len;
            file->chainCount++;
            #if 0
            fprintf (out, "%s%2d=(%d-%d)", i!=0?",":"", i, start, start+len);

            if (prog)
            {
                progBytes = readProg (prog, progBytes, start, len);
            }
            else
                dumpContents (start, len, header->recLen);
            #endif
        }
    }

}

static void readDirectory (DskInfo *info, int sector)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];

    fseek (info->fp, DSK_BYTES_PER_SECTOR * sector, SEEK_SET);
    fread (&data, 1, sizeof (data), info->fp);

    int i;
    for (i = 0; i < DSK_BYTES_PER_SECTOR/2; i++)
    {
        int sector = data[i][0] << 8 | data[i][1];

        if (sector == 0)
            break;
        // diskAnalyseFile (fp, sector, &headers[i]);

        DskFileInfo *file = &info->files[i];

        fseek (info->fp, DSK_BYTES_PER_SECTOR * sector, SEEK_SET);
        fread (&file->filehdr, 1, sizeof (DskFileHeader), info->fp);
        file->sector = sector;
        filesTI2Linux (file->filehdr.tiname, file->osname);
        decodeFileChains (file);

        if (file->filehdr.flags & 0x01)
        {
            file->length = (ntohs (file->filehdr.alloc) - 1) * DSK_BYTES_PER_SECTOR + file->filehdr.eof;
            // if (showBasic)
            //     prog = malloc (ntohs (header->alloc) * DSK_BYTES_PER_SECTOR);
        }
        else
            file->length = ntohs(file->filehdr.len);
    }

    info->fileCount = i;
}

void dskOutputVolumeHeader (DskInfo *info, FILE *out)
{
    fprintf (out,"Vol-Label='%-10.10s'", info->volhdr.tiname);
    fprintf (out,", sectors=%d", ntohs (info->volhdr.sectors));
    fprintf (out,", sec/trk:%d", info->volhdr.secPerTrk);
    fprintf (out,", DSR:'%-3.3s'", info->volhdr.dsk);
    fprintf (out,", Protect:'%c'", info->volhdr.protected);
    fprintf (out,", tracks:%d", info->volhdr.tracks);
    fprintf (out,", sides:%d", info->volhdr.sides);
    fprintf (out,", density:%02X", info->volhdr.density);

    if (info->volhdr.date[0])
        fprintf (out,", year:%-8.8s", info->volhdr.date);

    fprintf (out,", FAT sectors used");

    #if 1
    int max = 0x64;
    if (info->volhdr.sides==2)
    {
        if (info->volhdr.density == 1)
            max = 0x91;
        else
            max = 0xeb;
    }

    bool inuse = false;
    for (int i = 0; i <= max - 0x38; i++)
    {
        uint8_t map=info->volhdr.bitmap[i];

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

#if 0
void dskDumpContents (int sectorStart, int sectorCount, int recLen)
{
    if (!showContents)
        return;

    for (int i = sectorStart; i <= sectorStart+sectorCount; i++)
    {
        int8_t data[DSK_BYTES_PER_SECTOR];
        fseek (diskFp, DSK_BYTES_PER_SECTOR * i, SEEK_SET);

        fread (&data, sizeof (data), 1, diskFp);

        for (int j = 0; j < DSK_BYTES_PER_SECTOR; j += recLen)
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
#endif

static void printFileInfo (DskFileInfo *file, FILE *out)
{
    int length;

    DskFileHeader *header = &file->filehdr;
    fprintf (out, "%-10.10s", header->tiname);
    fprintf (out, " %6d", file->sector);

    if (header->flags & 0x01)
    {
        length = (ntohs (header->alloc) - 1) * DSK_BYTES_PER_SECTOR + header->eof;
        // if (showBasic)
        //     prog = malloc (ntohs (header->alloc) * DSK_BYTES_PER_SECTOR);
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

    // if (prog)
    //     decodeBasicProgram (prog, length);

    fprintf (out, "\n");
}

void dskOutputDirectory (DskInfo *info, FILE *out)
{
    // int count = diskAnalyseDirectory (disk, sector, headers);

    // if (count < 0)
    //     return count;

    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sectors\n");
    fprintf (out, "========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");

    for (int i = 0; i < info->fileCount; i++)
    {
        printFileInfo (&info->files[i], out);
    }
}

// int diskReadData (uint8_t *buff, int offset, int sectorStart, int sectorCount)
int dskReadFile (DskInfo *info, int index, uint8_t *buff, int offset, int len)
{
    int total = 0;

    DskFileInfo *file = &info->files[index];
    printf ("# req read %d bytes from file %d, chains=%d\n", len, index, file->chainCount);

    for (int i = 0; i < file->chainCount; i++)
    {
        printf ("# start=%d end=%d\n", file->chains[i].start, file->chains[i].end);
        for (int j = file->chains[i].start; j <= file->chains[i].end; j++)
        {
            if (offset > DSK_BYTES_PER_SECTOR)
            {
                offset -= DSK_BYTES_PER_SECTOR;
                continue;
            }

            int count = len;

            fseek (info->fp, DSK_BYTES_PER_SECTOR * j, SEEK_SET);

            if (offset + len > DSK_BYTES_PER_SECTOR)
                count = DSK_BYTES_PER_SECTOR - offset;

            printf ("read %d bytes from sector %d\n", count, j);
            fread (buff, 1, count, info->fp);
            buff += count;
            offset -= count;
            len -= count;
            total += count;
        }
    }

    return total;
}

int dskWriteFile (DskInfo *info, int index, uint8_t *buff, int offset, int len)
{
    return 0;
}

DskInfo *dskOpenVolume (const char *name)
{
    FILE *fp;

    if ((fp = fopen (name, "r")) == NULL)
    {
        printf ("Can't open %s\n", name);
        return NULL;
    }

    DskInfo *info = malloc (sizeof (DskInfo));
    info->fp = fp;

    fseek (fp, 0, SEEK_SET);
    fread (&info->volhdr, 1, sizeof (DskVolumeHeader), fp);
    filesTI2Linux (info->volhdr.tiname, info->osname);

    readDirectory (info, 1);

    return info;
}

void dskCloseVolume(DskInfo *info)
{
    fclose (info->fp);
}

