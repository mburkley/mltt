#incude "disksector.h"

DiskSector::DiskSector (uint8_t *volHdr, uint8_t *dirHdr, uint8_t *bitmap, FILE *fp)
{
    _volHdr = volHdr;
    _dirHdr = dirHdr;
    _bitmap = bitmap;
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

    for (i = start; i < sectorCount; i++)
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

    // volNeedsWrite = true;
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

    // volNeedsWrite = true;
}

int DiskSector::read (int sector, uint8_t data[], int offset, int len)
{
    fseek (_fp, DISK_BYTES_PER_SECTOR * sector + offset, SEEK_SET);
    printf ("read %d bytes from sector %d\n", count, sector);
    return fread (buff, 1, count, _fp);
}

int DiskSector::write (int sector, uint8_t data[], int offset, int len)
{
    fseek (_fp, DISK_BYTES_PER_SECTOR * sector + offset, SEEK_SET);
    printf ("# writing %d bytes to sector %d offset %d\n", count, sector, offset);
    return fwrite (buff, 1, len, _fp);
}

void DiskSector::sync ()
{
    if (_dirWriteNeeded)
    {
        write (DIR_HDR_SECTOR, _dirHdr, 0, DISK_BYTES_PER_SECTOR);
        _dirWriteNeeded = false;
        printf ("# Wrote dir\n");
    }

    if (_volWriteNeeded)
    {
        write (VOL_HDR_SECTOR, _volHdr, 0, DISK_BYTES_PER_SECTOR);
        _volWriteNeeded = false;
        printf ("# Wrote FAT\n");
    }

    fflush (fp);
}

void DiskSector::readDirSector ()
{
    read (DIR_HDR_SECTOR, _dirHdr, 0, DISK_BYTES_PER_SECTOR);
}

