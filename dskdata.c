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
#include <assert.h>

#include "files.h"
#include "dskdata.h"

static void decodeChain (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

static void encodeChain (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

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
        }
    }
}

static void encodeFileChains (DskFileInfo *file)
{
    // TODO why 23?

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=file->filehdr.chain[i];

        if (i < file->chainCount)
        {
            encodeChain (chain,
                         file->chains[i].start,
                         file->chains[i].end - file->chains[i].start);
        }
        else
        {
            chain[0] = chain[1] = chain[2] = 0;
        }
    }
}

static bool sectorIsFree (DskInfo *info, int sector)
{
    int byte = sector / 8;
    int bit = sector % 8;

    if (info->volhdr.bitmap[byte] & (1<<bit))
        return false;

    return true;
}

int dskFindFreeSector (DskInfo *info, int start)
{
    int i;

    for (i = start; i < info->sectorCount; i++)
    {
        if (sectorIsFree (info, i))
            return i;
    }

    return -1;
}

void dskAllocSectors (DskInfo *info, int start, int count)
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
    v->protect = ' ';
    v->tracks = 40;
    v->sides = 2;
    v->density = 1;
}

int dskCheckFileAccess (DskInfo *info, const char *path, int mode)
{
    // TODO check mode

    for (int i = 0; i < info->fileCount; i++)
    {
        printf ("compare %s %s\n", path, info->files[i].osname);
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

void dskFileTifiles (DskInfo *info, int index, Tifiles *header)
{
    header->secCount = info->files[index].filehdr.secCount;
    header->flags = info->files[index].filehdr.flags;
    header->recSec = info->files[index].filehdr.recSec;
    header->eofOffset = info->files[index].filehdr.eofOffset;
    header->recLen = info->files[index].filehdr.recLen;
    header->l3Alloc = info->files[index].filehdr.l3Alloc;
    memcpy (header->name, info->files[index].filehdr.tiname, 10);
}

int dskFileLength (DskInfo *info, int index)
{
    printf ("# length of file index %d is %d\n", index,
        info->files[index].length);
    return info->files[index].length;
}

int dskFileFlags (DskInfo *info, int index)
{
    return info->files[index].filehdr.flags;
}

int dskFileRecLen (DskInfo *info, int index)
{
    return info->files[index].filehdr.recLen;
}

bool dskFileProtected (DskInfo *info, int index)
{
    return info->files[index].filehdr.flags & FLAG_WP;
}

int dskFileCount (DskInfo *info)
{
    return info->fileCount;
}

int dskSectorCount (DskInfo *info)
{
    return info->sectorCount;
}

int dskSectorsFree (DskInfo *info)
{
    return info->sectorsFree;
}

/*  Insert a new file name into the directory maintaining alphabetic order */
int dskCreateFile (DskInfo *info, const char *path, Tifiles *header)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];

    fseek (info->fp, DSK_BYTES_PER_SECTOR * 1, SEEK_SET);
    fread (&data, 1, sizeof (data), info->fp);

    if (info->fileCount == MAX_FILE_COUNT)
        return -1;

    DskFileInfo *file = &info->files[info->fileCount];
    strncpy (file->osname, path, 10);
    file->osname[10] = 0;
    filesLinux2TI (file->osname, file->filehdr.tiname);

    /* Find free sector for directory entry */
    if ((file->sector = dskFindFreeSector (info, 2)) == -1)
        return -1;

    dskAllocSectors (info, file->sector, 1);
    file->length = 0;
    file->chainCount = 0;

    /*  Find where alphabetically the new file should go */
    int index;
    for (index = 0; index < info->fileCount; index++)
    {
        // TODO should compare TI names not os names
        if (strcmp (file->osname, info->files[index].osname) < 0)
        {
            printf ("# %s comes before %s; break;\n", file->osname,
                    info->files[index].osname);
            break;
        }
    }
             
    printf ("# hole punch at %d\n", index);
    /*  Make a hole in the dir alloc table */
    for (int j = DSK_BYTES_PER_SECTOR/2 - 1; j > index; j--)
    {
        data[j][0] = data[j-1][0];
        data[j][1] = data[j-1][1];
    }

    /*  Fill the hole with the new dir entry sector number */
    data[index][0] = file->sector >> 8;
    data[index][1] = file->sector & 0xff;

    /*  Populate the rest of the dir entry struct */
    // file->filehdr.len = 0;
    file->filehdr.secCount = header->secCount;
    file->filehdr.flags = header->flags;
    file->filehdr.recSec = header->recSec;
    file->filehdr.l3Alloc = header->l3Alloc;
    file->filehdr.eofOffset = header->eofOffset;
    file->filehdr.recLen = header->recLen;
    memset (file->filehdr.chain, 0, MAX_FILE_CHAINS * 3);

    /*  Write the updated volume header, dir entry and directory entry allocation sector */
    printf ("# write\n");
    fseek (info->fp, DSK_BYTES_PER_SECTOR * file->sector, SEEK_SET);
    fwrite (&file->filehdr, 1, sizeof (DskFileHeader), info->fp);
    fseek (info->fp, 0, SEEK_SET);
    fwrite (&info->volhdr, 1, DSK_BYTES_PER_SECTOR, info->fp);
    fwrite (&data, 1, sizeof (data), info->fp);

    return info->fileCount++;
}

#if 0
void diskAnalyseFile (FILE *fp, int sector, DiskFileHeader *header)
{
    int length;
    uint8_t *prog = NULL;
    int progBytes = 0;

}
#endif

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

        #if 0
        if (file->filehdr.flags & FLAG_PROG)
        {
            file->length = (be16toh (file->filehdr.secCount) - 1) * DSK_BYTES_PER_SECTOR + file->filehdr.eof;
            // if (showBasic)
            //     prog = malloc (ntohs (header->secCount) * DSK_BYTES_PER_SECTOR);
        }
        else
            file->length = be16toh (file->filehdr.len);
        #endif

        file->length = be16toh (file->filehdr.secCount) * DSK_BYTES_PER_SECTOR;

        if (file->filehdr.eofOffset != 0)
            file->length -= (DSK_BYTES_PER_SECTOR - file->filehdr.eofOffset);
    }

    info->fileCount = i;
}

void dskOutputVolumeHeader (DskInfo *info, FILE *out)
{
    fprintf (out,"Vol-Label='%-10.10s'", info->volhdr.tiname);
    fprintf (out,", sectors=%d", ntohs (info->volhdr.sectors));
    fprintf (out,", sec/trk:%d", info->volhdr.secPerTrk);
    fprintf (out,", DSR:'%-3.3s'", info->volhdr.dsk);
    fprintf (out,", Protect:'%c'", info->volhdr.protect);
    fprintf (out,", tracks:%d", info->volhdr.tracks);
    fprintf (out,", sides:%d", info->volhdr.sides);
    fprintf (out,", density:%02X", info->volhdr.density);

    if (info->volhdr.date[0])
        fprintf (out,", year:%-8.8s", info->volhdr.date);

    fprintf (out,"\nSectors allocated " );

    #if 1
    // TODO move the bitmap size calc out of output func
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
    // int length;

    DskFileHeader *header = &file->filehdr;
    fprintf (out, "%-10.10s", header->tiname);
    fprintf (out, " %6d", file->sector);

    #if 0
    if (header->flags & 0x01)
    {
        length = (ntohs (header->secCount) - 1) * DSK_BYTES_PER_SECTOR + header->eof;
        // if (showBasic)
        //     prog = malloc (ntohs (header->secCount) * DSK_BYTES_PER_SECTOR);
    }
    else
        length = be16toh(header->len);
    #endif

    fprintf (out, " %6d", file->length);

    fprintf (out, " %02X%-17.17s", header->flags,filesShowFlags (header->flags));
    fprintf (out, " %8d", be16toh(header->secCount));
    fprintf (out, " %8d", header->recSec * be16toh (header->secCount));
    fprintf (out, " %10d", header->eofOffset);
    fprintf (out, " %7d", le16toh (header->l3Alloc)); // NOTE : LE not BE
    fprintf (out, " %7d", header->recLen);

    for (int i = 0; i < file->chainCount; i++)
        fprintf (out, "%s%2d=(%d-%d)", i!=0?",":"", i, file->chains[i].start, file->chains[i].end);

    fprintf (out, "\n");
}

void dskOutputDirectory (DskInfo *info, FILE *out)
{
    // int count = diskAnalyseDirectory (disk, sector, headers);

    // if (count < 0)
    //     return count;

    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sector chains\n");
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
                printf ("# advance, offset now %d\n", offset);
                continue;
            }

            int count = len;

            fseek (info->fp, DSK_BYTES_PER_SECTOR * j + offset, SEEK_SET);

            /*  If the read spans more than one sector then read only up to the
             *  end of the currect sector for now in case the next sector is in
             *  a difference chain */
            if (offset + len > DSK_BYTES_PER_SECTOR)
                count = DSK_BYTES_PER_SECTOR - offset;

            printf ("read %d bytes from sector %d\n", count, j);
            fread (buff, 1, count, info->fp);
            buff += count;
            offset = 0;
            len -= count;
            total += count;
        }
    }

    return total;
}

int dskWriteFile (DskInfo *info, int index, uint8_t *buff, int offset, int len)
{
    int chain;
    int sector;
    DskFileInfo *file = &info->files[index];
    int endOfWrite = offset + len;

    /* secCount is the number of sectors that have been allocated and remain
     * available for write.  This is reduced by offset */
    int secCount = be16toh (file->filehdr.secCount);
    int total = 0;

    printf ("# req write %d bytes to file %d, chains=%d\n", len, index, file->chainCount);

    for (chain = 0; chain < file->chainCount; chain++)
    {
        printf ("# start=%d end=%d\n", file->chains[chain].start, file->chains[chain].end);
        for (sector = file->chains[chain].start; sector <= file->chains[chain].end; sector++)
        {
            if (offset < DSK_BYTES_PER_SECTOR)
                break;

            offset -= DSK_BYTES_PER_SECTOR;
            secCount--;
        }
    }

    assert (secCount != 0 || chain == file->chainCount);

    /*  We have found the position in the chain to start writing, which may at
     *  the end of the file.  If offset is non zero, or if offset+len is less
     *  than the end of the sector, then we need to do a read-modify-write (NO)*/
    while (len)
    {
        int count;

        if (secCount == 0)
        {
            /*  We have reached EOF.  Allocate a new sector */
            sector = dskFindFreeSector (info, 34); // 34th sector seems to be
            // standard for first data
            printf ("# Allocate new sector %d\n", sector);
            dskAllocSectors (info, sector, 1);
            secCount++;
            file->filehdr.secCount = htobe16 (be16toh (file->filehdr.secCount) + 1);

            /*  Is the new sector contiguous to the last sector in the last
             *  chain?  If so, then update the end of chain, otherwise create a
             *  new chain */
            if (chain > 0 && file->chains[chain-1].end + 1 == sector)
            {
                printf ("# Append to existing chain %d\n", chain-1);
                file->chains[chain-1].end = sector;
            }
            else
            {
                /*  TODO check if we run out of chains */
                printf ("# Allocate new chain %d\n", chain);
                file->chains[chain].start =
                file->chains[chain].end = sector;
                file->chainCount++;
                chain++;
            }
        }

        if (offset + len > DSK_BYTES_PER_SECTOR)
            count = DSK_BYTES_PER_SECTOR - offset;
        else
            count = len;

        fseek (info->fp, DSK_BYTES_PER_SECTOR * sector + offset, SEEK_SET);
        printf ("writing %d bytes to sector %d offset %d\n", count, sector, offset);
        total += fwrite (buff, 1, count, info->fp);

        offset -= count;
        len -= count;
        buff += count;

        /*  Advance to the next sector.  Check if it is still in the current
         *  chain */
        sector++;
        secCount--;

        if (len && secCount && sector > file->chains[chain].end)
        {
            printf ("# not at eof, but end of chain, advance to next chain\n");
            chain++;
            sector = file->chains[chain].start;
        }
    }

    if (endOfWrite > file->length)
    {
        file->length = endOfWrite;
        // file->filehdr.len = htobe16 (endOfWrite);
        file->filehdr.eofOffset = endOfWrite % DSK_BYTES_PER_SECTOR;
    }

    encodeFileChains (file);

    /*  Write the updated volume header and dir entry */
    printf ("# write\n");
    fseek (info->fp, DSK_BYTES_PER_SECTOR * file->sector, SEEK_SET);
    fwrite (&file->filehdr, 1, sizeof (DskFileHeader), info->fp);
    fseek (info->fp, 0, SEEK_SET);
    fwrite (&info->volhdr, 1, DSK_BYTES_PER_SECTOR, info->fp);

    return total;
}

DskInfo *dskOpenVolume (const char *name)
{
    FILE *fp;

    if ((fp = fopen (name, "r+")) == NULL)
    {
        printf ("Can't open %s\n", name);
        return NULL;
    }

    DskInfo *info = malloc (sizeof (DskInfo));
    info->fp = fp;

    fseek (fp, 0, SEEK_SET);
    fread (&info->volhdr, 1, sizeof (DskVolumeHeader), fp);
    filesTI2Linux (info->volhdr.tiname, info->osname);

    info->sectorCount = be16toh (info->volhdr.sectors);
    info->sectorsFree = 0;

    for (int i = 0; i < info->sectorCount; i++)
    {
        if (sectorIsFree (info, i))
            info->sectorsFree++;
    }

    printf ("# Volume %s has %d sectors (%d free)\n", info->osname,
            info->sectorCount, info->sectorsFree);
    readDirectory (info, 1);

    return info;
}

void dskCloseVolume(DskInfo *info)
{
    fclose (info->fp);
    free (info);
}

