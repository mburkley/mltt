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
 *  Create a virtual disk from a directory
 */

#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <dirent.h> 
#include <sys/stat.h>
#include <arpa/inet.h>

#define BYTES_PER_SECTOR        256
#define SECTORS_PER_DISK         720

unsigned char diskData[SECTORS_PER_DISK][BYTES_PER_SECTOR];

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
    uint8_t chain[76][3];
}
fileHeader;

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

static void encodeChain (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    // *p1 = (chain[1]&0xF)<<8|chain[0];
    // *p2 = chain[1]>>4|chain[2]<<4;

    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

// static uint8_t dirData[BYTES_PER_SECTOR/2][2];
// static int fileSectorsAlloc = 0;
static int dataSectorsAlloc = 32;
static int fileCount;

static void buildDirEnt (const char *name, const char *fname, int len)
{
    diskData[1][fileCount*2] = (fileCount+2) >> 8; 
    diskData[1][fileCount*2+1] = (fileCount+2) & 0xff;

    fileHeader *f = (fileHeader*) diskData[fileCount+2];
    f->flags = 0x01;
    strncpy (f->name, name, 10);
    int fileSecCount = len / BYTES_PER_SECTOR;
    f->alloc = htons(fileSecCount+1);
    f->eof = len - fileSecCount * BYTES_PER_SECTOR;
    // recSec
    // l3Alloc
    // recLen
    encodeChain (f->chain[0], dataSectorsAlloc, dataSectorsAlloc+fileSecCount);

    FILE *fp;

    if ((fp = fopen (fname, "r")) == NULL)
    {
        fprintf(stderr, "Can't read %s\n", name);
        exit(1);
    }

    fread (diskData[dataSectorsAlloc], 1, len, fp);
    fclose (fp);

    fileCount++;
    dataSectorsAlloc += fileSecCount;
}

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

    fwrite (diskData, BYTES_PER_SECTOR, SECTORS_PER_DISK, fp);
    fclose (fp);
}

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "directory?\n");
        exit(1);
    }

    DIR *d;
    struct dirent *dir;
    struct stat st;
    int size = 0;
    d = opendir(argv[1]);
    if (!d)
    {
        fprintf(stderr, "Can't read %s\n", argv[1]);
        exit(1);
    }
    while ((dir = readdir(d)) != NULL)
    {
        char fname[512];

        if (!strcmp (dir->d_name, "."))
            continue;

        if (!strcmp (dir->d_name, ".."))
            continue;

        sprintf (fname, "%s/%s", argv[1], dir->d_name);
        if (stat(fname, &st) != 0)
        {
            perror("stat");
            exit(1);
        }
        size += st.st_size;
        printf("%s %d %d\n", dir->d_name, (int) st.st_size, size);
        buildDirEnt (dir->d_name, fname, st.st_size);
    }
    closedir(d);
    printf ("total %d\n", size);

    if (size > (720-32)*BYTES_PER_SECTOR)
    {
        fprintf(stderr, "%s won't fit on a DSDD\n", argv[1]);
        exit(1);
    }
    buildVolumeHeader(argv[1]);

    writeDisk (argv[1]);
    return 0;
}

