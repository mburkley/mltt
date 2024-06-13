#ifndef __DISKSECTOR_H
#define __DISKSECTOR_H

#include <stdio.h>

#include "types.h"
#include "disk.h"

class DiskSector
{
public:
    DiskSector (uint8_t *bitmap, int sectorCount, FILE *fp);
    bool isFree (int sector);
    int findFree (int start);
    void alloc (int start, int count);
    void free (int start, int count);
    int read (int sector, uint8_t data[], int offset, int len);
    int write (int sector, uint8_t data[], int offset, int len);
    bool getBitmapChanged () { return _bitmapChanged; }
    void clearBitmapChanged () { _bitmapChanged = false; }

private:
    uint8_t *_bitmap;
    FILE *_fp;
    int _sectorCount;
    bool _bitmapChanged;
};

#endif

