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

#include "trace.h"
#include "diskdir.h"

static unsigned char diskData[SECTORS_PER_DISK][DISK_BYTES_PER_SECTOR];
static int dirSector;
static char *dirName;

typedef struct
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
    uint8_t chain[MAX_FILE_CHAINS][3];
}
fileHeader;

static bool fileExists[32];

typedef struct
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

static void buildVolumeHeader (const char *name)
{
    volumeHeader *v = (volumeHeader*) diskData[0];
    strncpy (v->name, name, 10);
    v->sectors = htons (SECTORS_PER_DISK);
    v->secPerTrk = 9;
    memcpy (v->dsk, "DSK", 3);
    v->protected = ' ';
    v->tracks = 40;
    v->sides = 2;
    v->density = 1;
}

static void fileLinux2TI (const char *lname, char tname[])
{
    int i;

    for (i = 0; i < 10; i++)
    {
        if (!lname[i])
            break;

        tname[i] = toupper (lname[i]);
    }

    for (; i < 10; i++)
        tname[i] = ' ';
}

static void fileTI2Linux (const char *tname, char *lname)
{
    int i;

    for (i = 0; i < 10; i++)
    {
        if (tname[i] == ' ')
            break;

        lname[i] = tname[i];
    }

    lname[i] = 0;
}

static void encodeChain (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    // *p1 = (chain[1]&0xF)<<8|chain[0];
    // *p2 = chain[1]>>4|chain[2]<<4;

    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

// TODO common funcs between this module and dumpdisk.c
static void decodeChain (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

static void allocBitMap (int start, int count)
{
    volumeHeader *v = (volumeHeader*) diskData[0];

    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        v->bitmap[byte] |= (1<<bit);
    }
}

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
    f->flags = 0x01;
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
    if (dirSector < 2 || dirSector > 34)
        return;

    // TODO should detect add chain to volume header but for now assume
    // range 2-34 is file headers
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

static void dirSelect (const char *name, bool readOnly)
{
    DIR *d;
    struct dirent *dir;
    struct stat st;
    int size = 0;
    d = opendir(name);
    ASSERT (d != NULL, "open directory");

    buildVolumeHeader(name);
    allocBitMap (0, 2);

    while ((dir = readdir(d)) != NULL)
    {
        char fname[512];

        if (!strcmp (dir->d_name, "."))
            continue;

        if (!strcmp (dir->d_name, ".."))
            continue;

        dirName = strdup (name);
        sprintf (fname, "%s/%s", dirName, dir->d_name);
        if (stat(fname, &st) != 0)
        {
            perror("stat");
            ASSERT (false, "can't stat file");
        }
        size += st.st_size;
        printf("%s %d %d\n", dir->d_name, (int) st.st_size, size);
        buildDirEnt (dir->d_name, fname, st.st_size);
    }
    closedir(d);
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


