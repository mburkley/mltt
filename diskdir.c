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
 *  Create a virtual disk in memory from a directory
 */

#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <ctype.h> 
#include <dirent.h> 
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/xattr.h>

#include "trace.h"
#include "diskdir.h"

static unsigned char diskData[SECTORS_PER_DISK][DISK_BYTES_PER_SECTOR];
static int dirSector;
static char dirName[512];

static bool fileExists[32];

// static uint8_t dirData[DISK_BYTES_PER_SECTOR/2][2];
// static int fileSectorsAlloc = 0;
static int dataSectorsAlloc = 32;
static int fileCount;

static void buildDirEnt (const char *name, const char *fname, int len)
{
    diskData[1][fileCount*2] = (fileCount+2) >> 8; 
    diskData[1][fileCount*2+1] = (fileCount+2) & 0xff;

    fileExists[fileCount] = true;
    fileHeader *f = (fileHeader*) diskData[fileCount+2];

    // Default flag is 0x01 - FIX-PROG.  If we see xattr of data or var we
    // change flags 0x80 and or 0x01
    f->flags = 0x01;

    char dataval[10];
    dataval[0]=0;
    int data = getxattr(fname, "user.data", dataval, 10);
    printf ("xattr %s %d %s\n", fname, data, dataval);
    if (data > 0 && dataval[0] == 'y')
        f->flags &= 0xFE;
    data = getxattr(fname, "user.var", dataval, 10);
    printf ("xattr %s %d %s\n", fname, data, dataval);
    if (data > 0 && dataval[0] == 'y')
        f->flags |= 0x80;

    fileLinux2TI (name, f->name);
    int fileSecCount = len / DISK_BYTES_PER_SECTOR;
    f->eof = len - fileSecCount * DISK_BYTES_PER_SECTOR;
    if (f->eof != 0)
        fileSecCount++;
    f->alloc = htons(fileSecCount);
    // recSec
    // l3Alloc
    // recLen
    encodeChain (f->chain[0], dataSectorsAlloc, fileSecCount-1);

    FILE *fp;

    if ((fp = fopen (fname, "r")) == NULL)
    {
        fprintf(stderr, "Can't read %s\n", name);
        exit(1);
    }

    fread (diskData[dataSectorsAlloc], 1, len, fp);
    fclose (fp);

    allocBitMap (fileCount+2,1);
    fileCount++;
    allocBitMap (dataSectorsAlloc, fileSecCount);
    dataSectorsAlloc += fileSecCount;
}

#if 1
static void writeDisk (const char *name)
{
    FILE *fp;
    char fname[32];

    sprintf (fname, "%s.dsk", name);
    if ((fp = fopen (fname, "w")) == NULL)
    {
        fprintf(stderr, "Can't write %s\n", fname);
        exit(1);
    }

    fwrite (diskData, DISK_BYTES_PER_SECTOR, SECTORS_PER_DISK, fp);
    fclose (fp);
}
#endif

static void dirSeek (int sector)
{
    dirSector = sector;
}

static void dirRead (unsigned char *buffer)
{
    memcpy (buffer, diskData[dirSector], DISK_BYTES_PER_SECTOR);
    printf ("read sector %d\n", dirSector);
}

/*  Write a sector.  Copies the sector to RAM and if the sector is in the
 *  directory entry range, it also updates a physical file on disk.  NOTE this
 *  assumes the TI99/4A updates a directory entry AFTER writing out all the data
 *  sectors for the file.
 */
static void dirWrite (unsigned char *buffer)
{
    memcpy (diskData[dirSector], buffer, DISK_BYTES_PER_SECTOR);
    printf ("write sector %d\n", dirSector);

    /*  We are only interested in updats to directory entries */
    if (dirSector < 2 || dirSector > 31)
        return;

    // TODO should detect add chain to volume header but for now assume
    // range 2-32 is file headers

    // TODO files are kept in alphabetic order.  Ensure reordering file names
    // does not break anyhting here.

    fileHeader *f = (fileHeader*) diskData[dirSector];

    FILE *fp;
    char fname[512];
    ASSERT (dirName != NULL, "dir name");
    strcpy (fname, dirName);
    strcat (fname, "/");
    fileTI2Linux (f->name, fname+strlen(fname));
    if (!fileExists[dirSector-2])
    {
        fp = fopen (fname, "w");
        ASSERT (fp != NULL, "Can't create new dir file");
        fileExists[dirSector-2] = true;
        printf ("Created file %s\n", fname);
    }
    else
    {
        fp = fopen (fname, "r+");
        ASSERT (fp != NULL, "Can't open dir file for update");
    }
    int length = (ntohs (f->alloc) - 1) * DISK_BYTES_PER_SECTOR + f->eof;

    if (f->eof == 0)
        length += DISK_BYTES_PER_SECTOR;

    // re-write the file from the image in memory
    for (int j = 0; j < MAX_FILE_CHAINS; j++)
    {
        uint16_t start, len;
        decodeChain (f->chain[j], &start, &len);
        if (start == 0)
            break;
        printf ("chain (%d to %d)\n", start,start+len);
        int count = DISK_BYTES_PER_SECTOR*(len+1);
        if (count > length)
            count = length;
        fwrite (diskData[start], count, 1, fp);
        length -= count;
    }
    fclose (fp);
}

static int dirCompare (const void *dir1, const void *dir2)
{
    return strcmp (((struct dirent*) dir1)->d_name, ((struct dirent*) dir2)->d_name);
}

static void dirSelect (const char *name, bool readOnly)
{
    DIR *d;
    struct dirent *dir;
    struct dirent fileList[128];
    struct stat st;
    int size = 0;
    int fileCount = 0;

    strcpy (dirName, name);
    buildVolumeHeader(name);
    allocBitMap (0, 2);

    d = opendir(name);
    ASSERT (d != NULL, "open directory");

    while ((dir = readdir(d)) != NULL)
    {
        if (!strcmp (dir->d_name, "."))
            continue;

        if (!strcmp (dir->d_name, ".."))
            continue;

        fileList[fileCount++] = *dir;
        ASSERT (fileCount < 128, "Too many files in disk dir");
    }
    closedir(d);

    qsort (fileList, fileCount, sizeof (struct dirent), dirCompare);

    for (int i = 0; i < fileCount; i++)
    {
        char fname[512];

        sprintf (fname, "%s/%s", dirName, fileList[i].d_name);
        if (stat(fname, &st) != 0)
        {
            perror("stat");
            ASSERT (false, "can't stat file");
        }
        size += st.st_size;
        printf("%s %d %d\n", fileList[i].d_name, (int) st.st_size, size);
        buildDirEnt (fileList[i].d_name, fname, st.st_size);
    }
    printf ("total %d\n", size);

    if (size > (720-32)*DISK_BYTES_PER_SECTOR)
    {
        fprintf(stderr, "%s won't fit on a DSDD\n", name);
        ASSERT (false, "dir too big");
    }

    writeDisk ("test");
}

static void dirDeselect(void)
{
// TODO
}

void diskDirLoad (int drive, bool readOnly, char *name)
{
    diskDriveHandler handler =
    {
        .seek = dirSeek,
        .read = dirRead,
        .write = dirWrite,
        .select = dirSelect,
        .deselect = dirDeselect,
        .readOnly = readOnly,
        .name = ""
    };

    strcpy (handler.name, name);

    diskRegisterDriveHandler (drive, &handler);
}


