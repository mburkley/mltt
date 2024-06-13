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

#ifndef __DISKVOLUME_H
#define __DISKVOLUME_H

// #include "files.h"
// #include "types.h"
#include <vector>

#include "disk.h"
#include "diskfile.h"
#include "disksector.h"

#define VOL_HDR_SECTOR          0
#define DIR_HDR_SECTOR          1
#define FIRST_DIR_ENTRY         2
#define FIRST_DATA_SECTOR      34

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
public:
    DiskVolume ();
    void encodeVolumeHeader (DiskVolumeHeader *vol, const char *name, int
                                secPerTrk, int tracks, int sides, int density);
    DiskFile *fileAccess (const char *path, int mode);
    DiskFile *fileOpen (const char *path, int mode);
    void fileClose (DiskFile *file);
    DiskFile *fileCreate (const char *path);
    bool fileUnlink (const char *path);
    void outputHeader (FILE *out);
    void outputDirectory (FILE *out);
    // int readFile (DiskFile *file, uint8_t *buff, int offset, int len);
    // int writeFile (DiskFile *file, uint8_t *buff, int offset, int len);
    int getFileCount () { return _fileCount; }
    int getSectorCount () { return _sectorCount; }
    int getSectorsFree () { return _sectorsFree; }
    bool open (const char *name);
    void close ();
    DiskFile *getFileByIndex (int ix)
    {
        // if (ix >= _files.size())
        //     return -1;

        return _files[ix];
    }
private:
    DiskVolumeHeader _volHdr;
    uint8_t _dirHdr[DISK_BYTES_PER_SECTOR/2][2];
    DiskSector *_sectors;
    std::string _osname;
    int _fileCount;
    int _sectorCount;
    int _sectorsFree;
    FILE *_fp;
    std::vector<DiskFile *> _files;
    int _lastInode;
    bool _volNeedsWrite;
    bool _dirNeedsWrite;

    void readDirectory ();
    void sync ();
    void freeFileResources (DiskFile *file);
    void addFileToList (DiskFile *file);
    void removeFileFromList (DiskFile *file);
    void updateDirectory ();
};

#endif

