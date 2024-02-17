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
 *  Defines floppy disk data structures
 */

#ifndef __DISKDATA_H
#define __DISKDATA_H

#include "types.h"

#define DSK_BYTES_PER_SECTOR   256
#define MAX_FILE_CHAINS         76
#define MAX_FILE_COUNT          128
#define DISK_FILENAME_MAX       1024
#define FILENAME_LEN            11

typedef struct
{
    char tiname[10];
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
DskFileHeader;

typedef struct
{
    char tiname[10];
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
DskVolumeHeader;

typedef struct
{
    DskFileHeader filehdr;
    char osname[FILENAME_LEN];
    int sector;
    int length;
    struct
    {
        int start;
        int end;
    }
    chains[MAX_FILE_CHAINS];
    int chainCount;
}
DskFileInfo;

typedef struct
{
    DskVolumeHeader volhdr;
    char osname[FILENAME_LEN];
    int fileCount;
    FILE *fp;
    int sectorMap[1];
    DskFileInfo files[MAX_FILE_COUNT];
}
DskInfo;

void diskDecodeChain (uint8_t chain[], uint16_t *p1, uint16_t *p2);
void diskAnalyseFirstSector (void);
void diskDumpContents (int sectorStart, int sectorCount, int recLen);
void diskAnalyseFile (int sector);
void diskAnalyseDirectory (int sector);

void dskOutputVolumeHeader (DskInfo *info, FILE *out);
void dskOutputDirectory (DskInfo *info, FILE *out);

int dskReadFile (DskInfo *info, int index, uint8_t *buff, int offset, int size);
int dskWriteFile (DskInfo *info, int index, uint8_t *buff, int offset, int size);
int dskCheckFileAccess (DskInfo *info, const char *path, int flags);
int dskCreateFile (DskInfo *info, const char *path, int mode);

DskInfo *dskOpenVolume (const char *name);
void dskCloseVolume(DskInfo *info);

int dskFileCount (DskInfo *info);
const char *dskFileName (DskInfo *info, int index);
int dskFileLength (DskInfo *info, int index);
bool dskFileProtected (DskInfo *info, int index);

#endif

