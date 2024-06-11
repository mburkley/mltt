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
#include "diskvolume.h"

#define FIRST_INODE 100
#define VOL_HDR_SECTOR 0
#define DIR_HDR_SECTOR 1
#define FIRST_DATA_SECTOR 34

bool DiskVolume::sectorIsFree (int sector)
{
    int byte = sector / 8;
    int bit = sector % 8;

    if (volhdr.bitmap[byte] & (1<<bit))
        return false;

    return true;
}

int DiskVolume::findFreeSector (int start)
{
    int i;

    for (i = start; i < sectorCount; i++)
    {
        if (sectorIsFree (i))
            return i;
    }

    return -1;
}

void DiskVolume::allocSectors (int start, int count)
{
    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        volhdr.bitmap[byte] |= (1<<bit);
    }

    volNeedsWrite = true;
}

void DiskVolume::freeSectors (int start, int count)
{
    printf ("# free %d sectors starting at %d\n", count, start);
    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        volhdr.bitmap[byte] &= ~(1<<bit);
    }

    volNeedsWrite = true;
}

/*  Insert a new file name into the directory maintaining alphabetic order.  If
 *  path is NULL, then add it to the end of the list */
DiskFile *DiskVolume::fileAdd (const char *path)
{
    DiskFile *newFile = new DiskFile (lastInode++);
    DiskFile *prevFile = nullptr;

    if (path)
        newFile->setName (path);

    /*  Find where alphabetically the new file should go.  Use the TI names for
     *  the comparison as the order must be as TI would expect. */
    for (DskFileInfo *file = firstFile; file != NULL; file = file->next)
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
        newFile->next = firstFile;
        firstFile = newFile;
    }

    dirNeedsWrite = true;
    fileCount++;
    return newFile;
}

/*  Remove a file from the file list and free its structure but don't deallocate
 *  its sectors */
void DiskVolume::fileRemove (DiskFile *removeFile)
{
    DskFileInfo *prevFile = NULL;
    /*  Remove the file from the file list */
    for (DskFileInfo *file = firstFile; file != NULL; file = file->next)
    {
        if (file == removeFile)
            break;

        prevFile = file;
    }

    /*  Remove the file from the file list */
    if (prevFile)
        prevFile->next = removeFile->next;
    else
        firstFile = removeFile->next;

    dirNeedsWrite = true;
    fileCount--;
    free (removeFile);
}

void DiskVolume::readDirectory (int sector)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];

    fseek (fp, DSK_BYTES_PER_SECTOR * sector, SEEK_SET);
    fread (&data, 1, sizeof (data), fp);

    lastInode = FIRST_INODE;

    for (int i = 0; i < DSK_BYTES_PER_SECTOR/2; i++)
    {
        int sector = data[i][0] << 8 | data[i][1];

        if (sector == 0)
            break;

        DiskFile *file = fileAdd (NULL);

        fseek (fp, DSK_BYTES_PER_SECTOR * sector, SEEK_SET);
        fread (&file->filehdr, 1, sizeof (DskFileHeader), fp);
        file->sector = sector;
        filesTI2Linux (file->filehdr.tiname, file->osname);
        file->unpackChains ();

        file->length = be16toh (file->filehdr.secCount) * DSK_BYTES_PER_SECTOR;

        if (file->filehdr.eofOffset != 0)
            file->length -= (DSK_BYTES_PER_SECTOR - file->filehdr.eofOffset);

        file->needsWrite = false;
    }
}

void DiskVolume::writeDirectory ()
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];
    memset (data, 0, DSK_BYTES_PER_SECTOR);

    int entry = 0;

    for (DskFileInfo *file = firstFile; file != NULL; file = file->next)
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
            fseek (fp, DSK_BYTES_PER_SECTOR * file->sector, SEEK_SET);
            fwrite (&file->filehdr, 1, sizeof (DskFileHeader), fp);
            file->needsWrite = false;
        }

        entry++;
    }

    if (dirNeedsWrite)
    {
        fseek (fp, DSK_BYTES_PER_SECTOR * DIR_HDR_SECTOR, SEEK_SET);
        fwrite (&data, 1, DSK_BYTES_PER_SECTOR, fp);
        dirNeedsWrite = false;
        printf ("# Wrote dir\n");
    }

    if (volNeedsWrite)
    {
        fseek (fp, DSK_BYTES_PER_SECTOR * VOL_HDR_SECTOR, SEEK_SET);
        fwrite (&volhdr, 1, DSK_BYTES_PER_SECTOR, fp);
        volNeedsWrite = false;
        printf ("# Wrote FAT\n");
    }

    fflush (fp);
}

/*  Free the sectors belonging to a file */
void DiskVolume::freeFileResources (DiskFile *file)
{
    printf ("# file remove dir sector %d\n", file->sector);
    /*  Walk the sector allocation chain for this file and free all sectors */
    for (int chain = 0; chain < file->chainCount; chain++)
    {
        printf ("# start=%d end=%d\n", file->chains[chain].start, file->chains[chain].end);
        freeSectors (file->chains[chain].start,
                        file->chains[chain].end - file->chains[chain].start + 1);
    }

    /*  Free the directory entry sector and free the file info struct */
    freeSectors (file->sector, 1);
    fileRemove (file);

    /*  Write the updated volume header, dir entry and directory entry allocation sector */
    printf ("# FAT write needed\n");
    volNeedsWrite = true;
    writeDirectory ();
}

void DiskVolume::encodeVolumeHeader (DiskVolumeHeader *vol, const char *name, int
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

DiskFile *DiskVolume::fileAccess (const char *path, int mode)
{
    // TODO check mode

    for (DiskFile *file = firstFile; file != NULL; file = file->next)
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

DskFileInfo *DiskVolume::fileOpen (const char *path, int mode)
{
    DiskFile *file = fileAccess (path, mode);

    if (file)
    {
        file->refCount++;
        return file;
    }

    return NULL;
}

DiskFile *DiskVolume::fileCreate (const char *path, Tifiles *header)
{
    /* Find free sector for directory entry */
    int dirSector;
    if ((dirSector = findFreeSector (2)) == -1)
    {
        printf ("# disk full, can't create file\n");
        return NULL;
    }

    DiskFile *file = fileAdd (path);

    allocSectors (dirSector, 1);
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
    dirNeedsWrite = true;
    writeDirectory ();

    return file;
}

/*  Remove a file name from the directory and free its sectors */
int DiskVolume::fileUnlink (const char *path)
{
    uint8_t data[DSK_BYTES_PER_SECTOR/2][2];

    fseek (fp, DSK_BYTES_PER_SECTOR * 1, SEEK_SET);
    fread (&data, 1, sizeof (data), fp);

    DskFileInfo *file;
    for (file = firstFile; file != NULL; file = file->next)
    {
        printf ("# compare %s to inode %d=%s\n", path, file->inode, file->osname);
        if (!strcmp (path, file->osname))
        {
            printf ("# %s found %s\n", __func__, path);
            break;
        }
    }
    // fseek (fp, DSK_BYTES_PER_SECTOR * 1, SEEK_SET);
    // fread (&data, 1, sizeof (data), fp);

    if (file == NULL)
    {
        printf ("# Can't find %s to unlink\n", path);
        return -1;
    }

    if (file->refCount == 0)
        freeFileResources (file);
    else
    {
        /*  File is still open somewhere.  Mark it as unlinked so it does not
         *  appear in any more listings and write out the directory sector
         *  without this file */
        file->unlinked = true;
        dirNeedsWrite = true;
        writeDirectory ();
    }

    return 0;
}

void DiskVolume::outputHeader (FILE *out)
{
    fprintf (out,"Vol-Label='%-10.10s'", volhdr.tiname);
    fprintf (out,", sectors=%d", ntohs (volhdr.sectors));
    fprintf (out,", sec/trk:%d", volhdr.secPerTrk);
    fprintf (out,", DSR:'%-3.3s'", volhdr.dsk);
    fprintf (out,", Protect:'%c'", volhdr.protect);
    fprintf (out,", tracks:%d", volhdr.tracks);
    fprintf (out,", sides:%d", volhdr.sides);
    fprintf (out,", density:%02X", volhdr.density);

    if (volhdr.date[0])
        fprintf (out,", year:%-8.8s", volhdr.date);

    fprintf (out,"\nSectors allocated " );

    #if 1
    // TODO move the bitmap size calc out of output func
    int max = 0x64;
    if (volhdr.sides==2)
    {
        if (volhdr.density == 1)
            max = 0x91;
        else
            max = 0xeb;
    }

    bool inuse = false;
    for (int i = 0; i <= max - 0x38; i++)
    {
        uint8_t map=volhdr.bitmap[i];

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

void DiskVolume::outputDirectory (FILE *out)
{
    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sector chains\n");
    fprintf (out, "========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");

    for (DskFileInfo *file = firstFile; file != NULL; file = file->next)
        printFileInfo (file, out);
}

DiskVolume *DiskVolume::open (const char *name)
{
    FILE *fp;

    if ((fp = fopen (name, "r+")) == NULL)
    {
        printf ("Can't open %s\n", name);
        return nullptr;
    }

    DiskVolume *vol = new DiskVolume;
    vol->fp = fp;

    fseek (fp, 0, SEEK_SET);
    fread (&vol->volhdr, 1, sizeof (DskVolumeHeader), fp);
    filesTI2Linux (vol->volhdr.tiname, vol->osname);

    vol->sectorCount = be16toh (vol->volhdr.sectors);
    vol->sectorsFree = 0;

    for (int i = 0; i < vol->sectorCount; i++)
    {
        if (vol->sectorIsFree (i))
            vol->sectorsFree++;
    }

    printf ("# Volume %s has %d sectors (%d free)\n", vol->osname,
            vol->sectorCount, vol->sectorsFree);
    vol->readDirectory (1);

    return vol;
}

void DiskVolume::close (DiskVolume *vol)
{
    fclose (vol->fp);
    delete vol;
}

