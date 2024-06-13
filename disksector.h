#ifndef __DISKSECTOR_H
#define __DISKSECTOR_H

#include <stdio.h>

#include "types.h"

#define VOL_HDR_SECTOR          0
#define DIR_HDR_SECTOR          1
#define FIRST_DIR_ENTRY         2
#define FIRST_DATA_SECTOR      34

class DiskSector
{
public:
    DiskSector (uint8_t *volHdr, uint8_t *dirHdr, uint8_t *bitmap, FILE *fp);
    bool isFree (int sector);
    int findFree (int start);
    void alloc (int start, int count);
    void free (int start, int count);
    int read (int sector, uint8_t data[], int offset, int len);
    int write (int sector, uint8_t data[], int offset, int len);
    void sync ();
    void setDirUpdate () { _dirWriteNeeded = true; }
    void setVolUpdate () { _volWriteNeeded = true; }
    void readDirSector ();

private:
    uint8_t *_volHdr;
    uint8_t *_dirHdr;
    uint8_t *_bitmap;
    FILE *_fp;
    bool _dirWriteNeeded;
    bool _volWriteNeeded;
};

#endif

