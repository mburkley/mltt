#include <string.h>

#include "diskfile.h"

DiskFile::DiskFile (DiskSector& sectorMap)
{
    _sectorMap = sectorMap;
}

void DiskFile::setName (const char *path)
{
    // strncpy (osname, path, 10);
    osname = path;
    filesLinux2TI (osname.c_str(), filehdr.tiname);
}

void DiskFile::setFlags (int flags)
{
    filehdr.flags = flags;
    needsWrite = true;
}

void DiskFile::setRecLen (int recLen)
{
    filehdr.recLen = recLen;
    filehdr.recSec = BYTES_PER_SECTOR / recLen;
    needsWrite = true;
}

/*  Free the sectors belonging to a file */
void DiskFile::freeResources ()
{
    if (_refCount != 0)
        return;

    printf ("# file remove dir sector %d\n", sector);
    /*  Walk the sector allocation chain for this file and free all sectors */
    for (int chain = 0; chain < chainCount; chain++)
    {
        printf ("# start=%d end=%d\n", chains[chain].start, chains[chain].end);
        _sectorMap->freeSectors (chains[chain].start,
                        chains[chain].end - chains[chain].start + 1);
    }

    /*  Free the directory entry sector and free the file info struct */
    _sectorMap->freeSectors (file->sector, 1);
    volume->fileRemove (file);

    /*  Write the updated volume header, dir entry and directory entry allocation sector */
    printf ("# FAT write needed\n");
    // volume->volNeedsWrite = true;
    _bitmap.setFatUpdate ();
    // volume->writeDirectory ();
}

void DiskFile::close ()
{
    _refCount--;

    if (unlinked)
        freeResources ();
        // TODO go through to sector and volume
}

void DiskFile::unlink()
{
    if (_refCount == 0)
        freeResources ();
    else
    {
        /*  File is still open somewhere.  Mark it as unlinked so it does not
         *  appear in any more listings and write out the directory sector
         *  without this file */
        _unlinked = true;
        _bitmap.setDirUpdate ();
        // TODO writeDirectory ();
    }
}

int DiskFile::seek (int offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET: pos = offset; break;
    case SEEK_CUR: pos += offset; break;
    case SEEK_END: pos = length + offset; break;
    default: printf ("# whence=%d ?\n", whence); break;
    }
    return pos;
}

void DiskFile::fromTifiles (Tifiles *header)
{
    header->secCount = filehdr.secCount;
    header->flags = filehdr.flags;
    header->recSec = filehdr.recSec;
    header->eofOffset = filehdr.eofOffset;
    header->recLen = filehdr.recLen;
    header->l3Alloc = filehdr.l3Alloc;
    memcpy (header->name, filehdr.tiname, 10);
}

void DiskFile::printInfo (FILE *out)
{
    DskFileHeader *header = &filehdr;
    fprintf (out, "%-10.10s", header->tiname);
    fprintf (out, " %6d", sector);

    fprintf (out, " %6d", length);

    fprintf (out, " %02X%-17.17s", header->flags,filesShowFlags (header->flags));
    fprintf (out, " %8d", be16toh(header->secCount));
    fprintf (out, " %8d", header->recSec * be16toh (header->secCount));
    fprintf (out, " %10d", header->eofOffset);
    fprintf (out, " %7d", le16toh (header->l3Alloc)); // NOTE : LE not BE ?
    fprintf (out, " %7d", header->recLen);

    for (int i = 0; i < chainCount; i++)
        fprintf (out, "%s%2d=(%d-%d)", i!=0?",":"", i, chains[i].start, chains[i].end);

    fprintf (out, "\n");
}

int DiskFile::read (uint8_t *buff, int offset, int len)
{
    int total = 0;

    printf ("# req read %d bytes from inode %d, chains=%d\n", len, inode, chainCount);

    for (int i = 0; i < chainCount; i++)
    {
        printf ("# start=%d end=%d\n", chains[i].start, chains[i].end);
        for (int j = chains[i].start; j <= chains[i].end; j++)
        {
            if (offset > DISK_BYTES_PER_SECTOR)
            {
                offset -= DISK_BYTES_PER_SECTOR;
                printf ("# advance, offset now %d\n", offset);
                continue;
            }

            int count = len;

            // TODO check for read past EOF

            /*  If the read spans more than one sector then read only up to the
             *  end of the currect sector for now in case the next sector is in
             *  a difference chain */
            if (offset + len > DISK_BYTES_PER_SECTOR)
                count = DISK_BYTES_PER_SECTOR - offset;

            _sector.read (j, buff, offset, count);
            buff += count;
            offset = 0;
            len -= count;
            total += count;
        }
    }

    return total;
}

int DiskFile::write (uint8_t *buff, int offset, int len)
{
    int chain;
    int sector;
    int newLength = offset + len;

    /* secCount is the number of sectors that have been allocated and remain
     * available for write.  This is reduced by offset */
    int secCount = be16toh (filehdr.secCount);
    int total = 0;

    printf ("# write file inode %d, chains=%d\n", inode, chainCount);
    printf ("# writing %d bytes at offset %d\n", len, offset);

    bool extendFile = true;
    for (chain = 0; chain < chainCount; chain++)
    {
        printf ("# chain=%d start=%d end=%d\n", chain, chains[chain].start, chains[chain].end);

        for (sector = chains[chain].start; sector <= chains[chain].end; sector++)
        {
            if (offset < DISK_BYTES_PER_SECTOR)
            {
                printf ("# offset %d begins in chain %d sector %d\n",
                        offset, chain, sector);
                extendFile = false;
                break;
            }

            offset -= DISK_BYTES_PER_SECTOR;
            secCount--;
        }

        if (!extendFile)
            break;
    }

    assert (secCount != 0 || chain == chainCount);

    /*  We have found the position in the chain to start writing, which may at
     *  the end of the file.  If offset is non zero, or if offset+len is less
     *  than the end of the sector, then we need to do a read-modify-write (NO!)*/
    while (len)
    {
        int count;

        if (secCount == 0)
        {
            /*  We have reached EOF.  Allocate a new sector */
            if ((sector = _sectorMap.findFree (FIRST_DATA_SECTOR)) == -1)
            {
                printf ("# disk full, can't write to file\n");
                break;
            }

            printf ("# Allocate new sector %d\n", sector);
            _sectorMap.alloc (sector, 1);
            secCount++;
            filehdr.secCount = htobe16 (be16toh (filehdr.secCount) + 1);

            /*  Is the new sector contiguous to the last sector in the last
             *  chain?  If so, then update the end of chain, otherwise create a
             *  new chain */
            if (chain > 0 && chains[chain-1].end + 1 == sector)
            {
                printf ("# Append to existing chain %d\n", chain-1);
                chains[chain-1].end = sector;
            }
            else
            {
                /*  TODO check if we run out of chains */
                printf ("# Allocate new chain %d\n", chain);
                chains[chain].start =
                chains[chain].end = sector;
                chainCount++;
                needsWrite = true;
                chain++;
            }
        }

        if (offset + len > DISK_BYTES_PER_SECTOR)
            count = DISK_BYTES_PER_SECTOR - offset;
        else
            count = len;

        total += _sector.write (sector, buff, offset, count);

        offset = 0;
        len -= count;
        buff += count;

        /*  Advance to the next sector.  Check if it is still in the current
         *  chain */
        sector++;
        secCount--;

        if (len && secCount && sector > chains[chain].end)
        {
            printf ("# not at eof, but end of chain, sector=%d end=%d\n",
                    sector, chains[chain].end);
            chain++;
            sector = chains[chain].start;
            printf ("# start of chaini %d sector=%d\n", chain, sector);
        }
    }

    if (newLength > length)
    {
        length = newLength;
        // filehdr.len = htobe16 (newLength);
        filehdr.eofOffset = newLength % DISK_BYTES_PER_SECTOR;
    }

    encodeFileChains (file);
    needsWrite = true;
    // writeDirectory ();

    return total;
}

DiskFile::readDirEnt (int sector)
{
    _bitmap.read (sector, &_filehdr, 0, DISK_BYTES_PER_SECTOR);
    _sector = sector;
    filesTI2Linux (filehdr.tiname, osname);
    unpackChains ();

    _length = be16toh (_filehdr.secCount) * DISK_BYTES_PER_SECTOR;

    if (file->filehdr.eofOffset != 0)
        file->length -= (DISK_BYTES_PER_SECTOR - file->filehdr.eofOffset);

    _needsWrite = false;
}

DiskFile::sync()
{
    if (_needsWrite)
    {
        printf ("# write dirent sector %d\n", file->sector);
        _bitmap.write (_sector, &_filehdr, 0, DISK_BYTES_PER_SECTOR);
        _needsWrite = false;
    }
}

