#include "disksector.h"

DiskSector::DiskSector (uint8_t *bitmap, int sectorCount, FILE *fp)
{
    _bitmap = bitmap;
    _sectorCount = sectorCount;
    _fp = fp;
}

bool DiskSector::isFree (int sector)
{
    int byte = sector / 8;
    int bit = sector % 8;

    if (_bitmap[byte] & (1<<bit))
        return false;

    return true;
}

int DiskSector::findFree (int start)
{
    int i;

    for (i = start; i < _sectorCount; i++)
    {
        if (isFree (i))
            return i;
    }

    return -1;
}

void DiskSector::alloc (int start, int count)
{
    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        _bitmap[byte] |= (1<<bit);
    }

    _bitmapChanged = true;
}

void DiskSector::free (int start, int count)
{
    printf ("# free %d sectors starting at %d\n", count, start);
    for (int i = start; i < start+count; i++)
    {
        int byte = i / 8;
        int bit = i % 8;
        _bitmap[byte] &= ~(1<<bit);
    }

    _bitmapChanged = true;
}

int DiskSector::read (int sector, uint8_t data[], int offset, int len)
{
    fseek (_fp, DISK_BYTES_PER_SECTOR * sector + offset, SEEK_SET);
    printf ("read %d bytes from sector %d\n", len, sector);
    return fread (data, 1, len, _fp);
}

int DiskSector::write (int sector, uint8_t data[], int offset, int len)
{
    fseek (_fp, DISK_BYTES_PER_SECTOR * sector + offset, SEEK_SET);
    printf ("# writing %d bytes to sector %d offset %d\n", len, sector, offset);
    return fwrite (data, 1, len, _fp);
}

