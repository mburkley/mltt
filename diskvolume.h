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

#include "files.h"
#include "types.h"

#define DSK_BYTES_PER_SECTOR    256
#define MAX_FILE_CHAINS         76
#define MAX_FILE_COUNT          128
#define DISK_FILENAME_MAX       1024
#define FILENAME_LEN            11

typedef struct
{
    char tiname[10];
    int16_t sectors;
    uint8_t secPerTrk;
    char dsk[3];
    // 0x10
    uint8_t protect; // 'P' = protected, ' '=not
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
DiskVolumeHeader;

class DiskVolume
{
private:
    DskVolumeHeader volhdr;
    char osname[FILENAME_LEN+100]; // TODO
    int fileCount;
    int sectorCount;
    int sectorsFree;
    FILE *fp;
    // DskFileInfo files[MAX_FILE_COUNT];
    DiskFile *firstFile;
    int lastInode;
    bool volNeedsWrite;
    bool dirNeedsWrite;
    bool sectorIsFree (int sector);
    int findFreeSector (int start);
    void allocSectors (int start, int count);
    void freeSectors (int start, int count);
    DiskFile *fileAdd (const char *path);
    void fileRemove (DiskFile *removeFile);
    void readDirectory (int sector);
    void writeDirectory ();
    void freeFileResources (DiskFile *file);
public:
    void encodeVolumeHeader (DiskVolumeHeader *vol, const char *name, int
                                secPerTrk, int tracks, int sides, int density);
    DiskFile *fileAccess (const char *path, int mode);
    DiskFile *fileOpen (const char *path, int mode);
    void fileClose (DiskFile *file);
    DiskFile *fileCreate (const char *path, Tifiles *header);
    int fileUnlink (const char *path);
    void outputHeader (FILE *out);
    void outputDirectory (FILE *out);
    int readFile (DiskFile *file, uint8_t *buff, int offset, int len);
    int writeFile (DskFileInfo *file, uint8_t *buff, int offset, int len);
    int fileCount () { return fileCount; }
    int sectorCount () { return sectorCount; }
    int sectorsFree () { return sectorsFree; }
    DiskFile *fileFirst () { return info->firstFile; }
    static DiskVolume *open (const char *name);
    static void close (DiskVolume *vol);
};

#endif

