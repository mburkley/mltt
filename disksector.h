#ifndef __DISKSECTOR_H
#define __DISKSECTOR_H

#include <stdio.h>

#include "types.h"
#include "disk.h"

class DiskSector
{
public:
    DiskSector (uint8_t *bitmap, uint8_t *bitmapSectorData, int bitmapSector, FILE *fp);
    void setSectorCount (int count) { _sectorCount = count; }
    bool isFree (int sector);
    int findFree (int start);
    void alloc (int start, int count);
    void free (int start, int count);
    int read (int sector, uint8_t data[], int offset, int len);
    int write (int sector, uint8_t data[], int offset, int len);
    void sync ();

private:
    uint8_t *_bitmap;           // The allocation bitmap ("FAT")
    int _bitmapSector;          // The sector it is contained it
    uint8_t *_bitmapSectorData;  // The whole sector
    FILE *_fp;
    int _sectorCount;
    bool _bitmapChanged;
};

#endif

