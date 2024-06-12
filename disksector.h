#include <stdio.h>

#include "types.h"

class DiskSector
{
public:
    DiskSector (uint8_t bitmap, FILE *fp);
    bool isFree (int sector);
    int findFree (int start);
    void alloc (int start, int count);
    void free (int start, int count);
    int read (int sector, uint8_t data[], int offset, int len);
    int write (int sector, uint8_t data[], int offset, int len);
    void sync ();
    void setDirUpdate () { _dirWriteNeeded = true; }
    void setVolUpdate () { _volWriteNeeded = true; }

private
    uint8_t *_volHdr;
    uint8_t *_dirHdr;
    uint8_t *_bitmap;
    FILE *_fp;
    bool _dirWriteNeeded;
    bool _volWriteNeeded;
};
