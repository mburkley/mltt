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
 *
 *  This should REALLY have been in C++ ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "files.h"
#include "dskdata.h"

#define FIRST_INODE 100
#define VOL_HDR_SECTOR 0
#define DIR_HDR_SECTOR 1
#define FIRST_DATA_SECTOR 34

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

static int dskFindFreeSector (DskInfo *info, int start)
{
    int i;

    for (i = start; i < info->sectorCount; i++)
    {
        if (sectorIsFree (info, i))
            return i;
    }

    return -1;
}

static void dskAllocSectors (DskInfo *info, int start, int count)
{
    DskVolumeHeader *v = &info->volhdr;

    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        v->bitmap[byte] |= (1<<bit);
    }

    info->volNeedsWrite = true;
}

static void dskFreeSectors (DskInfo *info, int start, int count)
{
    DskVolumeHeader *v = &info->volhdr;

    printf ("# free %d sectors starting at %d\n", count, start);
    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        v->bitmap[byte] &= ~(1<<bit);
    }

    info->volNeedsWrite = true;
}

/*  Insert a new file name into the directory maintaining alphabetic order.  If
 *  path is NULL, then add it to the end of the list */
static DskFileInfo *fileAdd (DskInfo *info, const char *path)
{
    DskFileInfo *newFile = calloc (1, sizeof (DskFileInfo));
    DskFileInfo *prevFile = NULL;

    newFile->inode = info->lastInode++;

    if (path)
    {
        strncpy (newFile->osname, path, 10);
        filesLinux2TI (newFile->osname, newFile->filehdr.tiname);
    }

    /*  Find where alphabetically the new file should go.  Use the TI names for
     *  the comparison as the order must be as TI would expect. */
    for (DskFileInfo *file = info->firstFile; file != NULL; file = file->next)
    {
        if (path && strcmp (newFile->filehdr.tiname, file->filehdr.tiname) < 0)
        {
            printf ("# %s comes before %s; break;\n", newFile->filehdr.tiname,
                    file->filehdr.tiname);
            break;
        }

        prevFile = file;
    }

    /*  Add the file to the file list */
    if (prevFile)
    {
        newFile->next = prevFile->next;
        prevFile->next = newFile;
    }
    else
    {
        newFile->next = info->firstFile;
        info->firstFile = newFile;
    }

    info->dirNeedsWrite = true;
    info->fileCount++;
    return newFile;
}

/*  Remove a file from the file list and free its structure but don't deallocate
 *  its sectors */
static void fileRemove (DskInfo *info, DskFileInfo *removeFile)
{
    DskFileInfo *prevFile = NULL;
    /*  Remove the file from the file list */
    for (DskFileInfo *file = info->firstFile; file != NULL; file = file->next)
    {
        if (file == removeFile)
            break;

        prevFile = file;
    }

    /*  Remove the file from the file list */
    if (prevFile)
        prevFile->next = removeFile->next;
    else
        info->firstFile = removeFile->next;

    info->dirNeedsWrite = true;
    info->fileCount--;
    free (removeFile);
}

static void readDirectory (DskInfo *info, int sector)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];

    fseek (info->fp, DSK_BYTES_PER_SECTOR * sector, SEEK_SET);
    fread (&data, 1, sizeof (data), info->fp);

    info->lastInode = FIRST_INODE;

    for (int i = 0; i < DSK_BYTES_PER_SECTOR/2; i++)
    {
        int sector = data[i][0] << 8 | data[i][1];

        if (sector == 0)
            break;

        DskFileInfo *file = fileAdd (info, NULL);

        fseek (info->fp, DSK_BYTES_PER_SECTOR * sector, SEEK_SET);
        fread (&file->filehdr, 1, sizeof (DskFileHeader), info->fp);
        file->sector = sector;
        filesTI2Linux (file->filehdr.tiname, file->osname);
        decodeFileChains (file);

        file->length = be16toh (file->filehdr.secCount) * DSK_BYTES_PER_SECTOR;

        if (file->filehdr.eofOffset != 0)
            file->length -= (DSK_BYTES_PER_SECTOR - file->filehdr.eofOffset);

        file->needsWrite = false;
    }
}

static void writeDirectory (DskInfo *info)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];
    memset (data, 0, DSK_BYTES_PER_SECTOR);

    int entry = 0;

    for (DskFileInfo *file = info->firstFile; file != NULL; file = file->next)
    {
        /*  Files that have been unlinked still exist but do not get an entry in
         *  the directory sector */
        if (file->unlinked)
            continue;

        data[entry][0] = file->sector >> 8;
        data[entry][1] = file->sector & 0xff;

        if (file->needsWrite)
        {
            printf ("# write dirent sector %d\n", file->sector);
            fseek (info->fp, DSK_BYTES_PER_SECTOR * file->sector, SEEK_SET);
            fwrite (&file->filehdr, 1, sizeof (DskFileHeader), info->fp);
            file->needsWrite = false;
        }

        entry++;
    }

    if (info->dirNeedsWrite)
    {
        fseek (info->fp, DSK_BYTES_PER_SECTOR * DIR_HDR_SECTOR, SEEK_SET);
        fwrite (&data, 1, DSK_BYTES_PER_SECTOR, info->fp);
        info->dirNeedsWrite = false;
        printf ("# Wrote dir\n");
    }

    if (info->volNeedsWrite)
    {
        fseek (info->fp, DSK_BYTES_PER_SECTOR * VOL_HDR_SECTOR, SEEK_SET);
        fwrite (&info->volhdr, 1, DSK_BYTES_PER_SECTOR, info->fp);
        info->volNeedsWrite = false;
        printf ("# Wrote FAT\n");
    }

    fflush (info->fp);
}

/*  Free the sectors belonging to a file */
static void freeFileResources (DskInfo *info, DskFileInfo *file)
{
    printf ("# file remove dir sector %d\n", file->sector);
    /*  Walk the sector allocation chain for this file and free all sectors */
    for (int chain = 0; chain < file->chainCount; chain++)
    {
        printf ("# start=%d end=%d\n", file->chains[chain].start, file->chains[chain].end);
        dskFreeSectors (info,
                        file->chains[chain].start,
                        file->chains[chain].end - file->chains[chain].start + 1);
    }

    /*  Free the directory entry sector and free the file info struct */
    dskFreeSectors (info, file->sector, 1);
    fileRemove (info, file);

    /*  Write the updated volume header, dir entry and directory entry allocation sector */
    printf ("# FAT write needed\n");
    info->volNeedsWrite = true;
    writeDirectory (info);
}

void dskEncodeVolumeHeader (DskVolumeHeader *vol, const char *name, int
                            secPerTrk, int tracks, int sides, int density)
{
    filesLinux2TI (name, vol->tiname);
    // v->sectors = htons (SECTORS_PER_DISK);
    int sectors = tracks * secPerTrk * sides;
    vol->sectors = htobe16 (sectors);
    vol->secPerTrk = secPerTrk;
    memcpy (vol->dsk, "DSK", 3);
    vol->protect = ' ';
    vol->tracks = tracks;
    vol->sides = sides;
    vol->density = density;
}

DskFileInfo *dskFileAccess (DskInfo *info, const char *path, int mode)
{
    // TODO check mode

    for (DskFileInfo *file = info->firstFile; file != NULL; file = file->next)
    {
        printf ("compare %s %s\n", path, file->osname);
        if (!strcmp (path, file->osname))
        {
            printf ("# %s %s inode=%d\n", __func__, path, file->inode);
            return file;
        }
    }

    return NULL;
}

DskFileInfo *dskFileOpen (DskInfo *info, const char *path, int mode)
{
    DskFileInfo *file = dskFileAccess (info, path, mode);

    if (file)
    {
        file->refCount++;
        return file;
    }

    return NULL;
}

void dskFileClose (DskInfo *info, DskFileInfo *file)
{
    file->refCount--;

    if (file->refCount == 0 && file->unlinked)
        freeFileResources (info, file);
}

int dskFileSeek (DskInfo *info, DskFileInfo *file, int offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET: file->pos = offset; break;
    case SEEK_CUR: file->pos += offset; break;
    case SEEK_END: file->pos = file->length + offset; break;
    default: printf ("# whence=%d ?\n", whence); break;
    }
    return file->pos;
}

const char *dskFileName (DskInfo *info, DskFileInfo *file)
{
    return file->osname;
}

int dskFileInode (DskInfo *info, DskFileInfo *file)
{
    return file->inode;
}

void dskFileTifiles (DskInfo *info, DskFileInfo *file, Tifiles *header)
{
    header->secCount = file->filehdr.secCount;
    header->flags = file->filehdr.flags;
    header->recSec = file->filehdr.recSec;
    header->eofOffset = file->filehdr.eofOffset;
    header->recLen = file->filehdr.recLen;
    header->l3Alloc = file->filehdr.l3Alloc;
    memcpy (header->name, file->filehdr.tiname, 10);
}

int dskFileLength (DskInfo *info, DskFileInfo *file)
{
    printf ("# length of file inode %d is %d\n", file->inode,
        file->length);
    return file->length;
}

int dskFileFlags (DskInfo *info, DskFileInfo *file)
{
    return file->filehdr.flags;
}

int dskFileRecLen (DskInfo *info, DskFileInfo *file)
{
    return file->filehdr.recLen;
}

void dskFileFlagsSet (DskInfo *info, DskFileInfo *file, int flags)
{
    file->filehdr.flags = flags;
    file->needsWrite = true;
    writeDirectory (info);
}

void dskFileRecLenSet (DskInfo *info, DskFileInfo *file, int recLen)
{
    file->filehdr.recLen = recLen;
    file->filehdr.recSec = BYTES_PER_SECTOR / recLen;
    file->needsWrite = true;
    writeDirectory (info);
}

int dskFileSecCount (DskInfo *info, DskFileInfo *file)
{
    return file->filehdr.secCount;
}

bool dskFileProtected (DskInfo *info, DskFileInfo *file)
{
    return file->filehdr.flags & FLAG_WP;
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

DskFileInfo *dskFileFirst (DskInfo *info)
{
    return info->firstFile;
}

DskFileInfo *dskFileNext (DskInfo *info, DskFileInfo *file)
{
    return file->next;
}

DskFileInfo *dskCreateFile (DskInfo *info, const char *path, Tifiles *header)
{
    /* Find free sector for directory entry */
    int dirSector;
    if ((dirSector = dskFindFreeSector (info, 2)) == -1)
    {
        printf ("# disk full, can't create file\n");
        return NULL;
    }

    DskFileInfo *file = fileAdd (info, path);

    dskAllocSectors (info, dirSector, 1);
    file->sector = dirSector;
             
    /*  Populate the rest of the dir entry struct */
    // file->filehdr.len = 0;
    file->filehdr.secCount = 0; // header->secCount;
    file->filehdr.flags = header->flags;
    file->filehdr.recSec = header->recSec;
    file->filehdr.l3Alloc = header->l3Alloc;
    file->filehdr.eofOffset = header->eofOffset;
    file->filehdr.recLen = header->recLen;
    file->needsWrite = true;

    file->refCount++;
    file->needsWrite = true;
    info->dirNeedsWrite = true;
    writeDirectory (info);

    return file;
}

/*  Remove a file name from the directory and free its sectors */
int dskUnlinkFile (DskInfo *info, const char *path)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];

    fseek (info->fp, DSK_BYTES_PER_SECTOR * 1, SEEK_SET);
    fread (&data, 1, sizeof (data), info->fp);

    DskFileInfo *file;
    for (file = info->firstFile; file != NULL; file = file->next)
    {
        printf ("# compare %s to inode %d=%s\n", path, file->inode, file->osname);
        if (!strcmp (path, file->osname))
        {
            printf ("# %s found %s\n", __func__, path);
            break;
        }
    }
    // fseek (info->fp, DSK_BYTES_PER_SECTOR * 1, SEEK_SET);
    // fread (&data, 1, sizeof (data), info->fp);

    if (file == NULL)
    {
        printf ("# Can't find %s to unlink\n", path);
        return -1;
    }

    if (file->refCount == 0)
        freeFileResources (info, file);
    else
    {
        /*  File is still open somewhere.  Mark it as unlinked so it does not
         *  appear in any more listings and write out the directory sector
         *  without this file */
        file->unlinked = true;
        info->dirNeedsWrite = true;
        writeDirectory (info);
    }

    return 0;
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

static void printFileInfo (DskFileInfo *file, FILE *out)
{
    DskFileHeader *header = &file->filehdr;
    fprintf (out, "%-10.10s", header->tiname);
    fprintf (out, " %6d", file->sector);

    fprintf (out, " %6d", file->length);

    fprintf (out, " %02X%-17.17s", header->flags,filesShowFlags (header->flags));
    fprintf (out, " %8d", be16toh(header->secCount));
    fprintf (out, " %8d", header->recSec * be16toh (header->secCount));
    fprintf (out, " %10d", header->eofOffset);
    fprintf (out, " %7d", le16toh (header->l3Alloc)); // NOTE : LE not BE ?
    fprintf (out, " %7d", header->recLen);

    for (int i = 0; i < file->chainCount; i++)
        fprintf (out, "%s%2d=(%d-%d)", i!=0?",":"", i, file->chains[i].start, file->chains[i].end);

    fprintf (out, "\n");
}

void dskOutputDirectory (DskInfo *info, FILE *out)
{
    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sector chains\n");
    fprintf (out, "========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");

    for (DskFileInfo *file = info->firstFile; file != NULL; file = file->next)
        printFileInfo (file, out);
}

int dskReadFile (DskInfo *info, DskFileInfo *file, uint8_t *buff, int offset, int len)
{
    int total = 0;

    printf ("# req read %d bytes from inode %d, chains=%d\n", len, file->inode, file->chainCount);

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

int dskWriteFile (DskInfo *info, DskFileInfo *file, uint8_t *buff, int offset, int len)
{
    int chain;
    int sector;
    int newLength = offset + len;

    /* secCount is the number of sectors that have been allocated and remain
     * available for write.  This is reduced by offset */
    int secCount = be16toh (file->filehdr.secCount);
    int total = 0;

    printf ("# write file inode %d, chains=%d\n", file->inode, file->chainCount);
    printf ("# writing %d bytes at offset %d\n", len, offset);

    bool extendFile = true;
    for (chain = 0; chain < file->chainCount; chain++)
    {
        printf ("# chain=%d start=%d end=%d\n", chain, file->chains[chain].start, file->chains[chain].end);

        for (sector = file->chains[chain].start; sector <= file->chains[chain].end; sector++)
        {
            if (offset < DSK_BYTES_PER_SECTOR)
            {
                printf ("# offset %d begins in chain %d sector %d\n",
                        offset, chain, sector);
                extendFile = false;
                break;
            }

            offset -= DSK_BYTES_PER_SECTOR;
            secCount--;
        }

        if (!extendFile)
            break;
    }

    assert (secCount != 0 || chain == file->chainCount);

    /*  We have found the position in the chain to start writing, which may at
     *  the end of the file.  If offset is non zero, or if offset+len is less
     *  than the end of the sector, then we need to do a read-modify-write (NO!)*/
    while (len)
    {
        int count;

        if (secCount == 0)
        {
            /*  We have reached EOF.  Allocate a new sector */
            if ((sector = dskFindFreeSector (info, FIRST_DATA_SECTOR)) == -1)
            {
                printf ("# disk full, can't write to file\n");
                break;
            }

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
                file->needsWrite = true;
                chain++;
            }
        }

        if (offset + len > DSK_BYTES_PER_SECTOR)
            count = DSK_BYTES_PER_SECTOR - offset;
        else
            count = len;

        fseek (info->fp, DSK_BYTES_PER_SECTOR * sector + offset, SEEK_SET);
        printf ("# writing %d bytes to sector %d offset %d\n", count, sector, offset);
        total += fwrite (buff, 1, count, info->fp);

        offset = 0;
        len -= count;
        buff += count;

        /*  Advance to the next sector.  Check if it is still in the current
         *  chain */
        sector++;
        secCount--;

        if (len && secCount && sector > file->chains[chain].end)
        {
            printf ("# not at eof, but end of chain, sector=%d end=%d\n",
                    sector, file->chains[chain].end);
            chain++;
            sector = file->chains[chain].start;
            printf ("# start of chaini %d sector=%d\n", chain, sector);
        }
    }

    if (newLength > file->length)
    {
        file->length = newLength;
        // file->filehdr.len = htobe16 (newLength);
        file->filehdr.eofOffset = newLength % DSK_BYTES_PER_SECTOR;
    }

    encodeFileChains (file);
    file->needsWrite = true;
    writeDirectory (info);

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

    DskInfo *info = calloc (1, sizeof (DskInfo));
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

