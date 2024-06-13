#include <cstring>
#include <cassert>

#include "diskfile.h"

DiskFile::DiskFile (const char *path, DiskSector *sectorMap, int dirSector, int inode)
{
    _sectorMap = sectorMap;
    _dirSector = dirSector;
    _inode = inode;
    _osname = path;
    filesLinux2TI (_osname.c_str(), _filehdr.tiname);
}

void DiskFile::decodeOneChain (uint8_t chain[], uint16_t *p1, uint16_t *p2)
{
    *p1 = (chain[1]&0xF)<<8|chain[0];
    *p2 = chain[1]>>4|chain[2]<<4;
}

void DiskFile::encodeOneChain (uint8_t chain[], uint16_t p1, uint16_t p2)
{
    chain[0] = p1 & 0xff;
    chain[1] = ((p1 >> 8) & 0x0F)|((p2&0x0f)<<4);
    chain[2] = p2 >> 4;
}

void DiskFile::decodeChain ()
{
    // TODO why 23?
    _chainCount = 0;

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=_filehdr.chain[i];

        if (chain[0] != 0 || chain[1] != 0 || chain[2] !=0)
        {
            uint16_t start, len;
            decodeOneChain (chain, &start, &len);

            _chains[_chainCount].start = start;
            _chains[_chainCount].end = start+len;
            _chainCount++;
        }
    }
}

void DiskFile::encodeChain ()
{
    // TODO why 23?

    for (int i = 0; i < 23; i++)
    {
        uint8_t *chain=_filehdr.chain[i];

        if (i < _chainCount)
        {
            encodeOneChain (chain,
                         _chains[i].start,
                         _chains[i].end - _chains[i].start);
        }
        else
        {
            chain[0] = chain[1] = chain[2] = 0;
        }
    }
}


// void DiskFile::setName (const char *path)
// {
    // strncpy (osname, path, 10);
// }

void DiskFile::setFlags (int flags)
{
    _filehdr.flags = flags;
    _needsWrite = true;
}

void DiskFile::setRecLen (int recLen)
{
    _filehdr.recLen = recLen;
    _filehdr.recSec = DISK_BYTES_PER_SECTOR / recLen;
    _needsWrite = true;
}

/*  Free the sectors belonging to a file */
void DiskFile::freeResources ()
{
    if (_refCount != 0)
        return;

    printf ("# file remove dir sector %d\n", _dirSector);
    /*  Walk the sector allocation chain for this file and free all sectors */
    for (int chain = 0; chain < _chainCount; chain++)
    {
        printf ("# start=%d end=%d\n", _chains[chain].start, _chains[chain].end);
        _sectorMap->free (_chains[chain].start,
                          _chains[chain].end - _chains[chain].start + 1);
    }

    /*  Free the directory entry sector and free the file info struct */
    _sectorMap->free (_dirSector, 1);
    // volume->fileRemove (file);

    /*  Write the updated volume header, dir entry and directory entry allocation sector */
    printf ("# FAT write needed\n");
    // volume->volNeedsWrite = true;
    _sectorMap->setVolUpdate ();
    // volume->writeDirectory ();
}

void DiskFile::close ()
{
    _refCount--;

    if (_unlinked)
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
        _sectorMap->setDirUpdate ();
        // TODO writeDirectory ();
    }
}

int DiskFile::seek (int offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET: _pos = offset; break;
    case SEEK_CUR: _pos += offset; break;
    case SEEK_END: _pos = _length + offset; break;
    default: printf ("# whence=%d ?\n", whence); break;
    }
    return _pos;
}

bool DiskFile::create ()
{
    /* Find free sector for directory entry */
    if ((_dirSector = _sectorMap->findFree (FIRST_DIR_ENTRY)) == -1)
    {
        printf ("# disk full, can't create file\n");
        return false;
    }

    _refCount++;
    _sectorMap->alloc (_dirSector, 1);
    return true;
}

void DiskFile::toTifiles (Tifiles *header)
{
    header->secCount = _filehdr.secCount;
    header->flags = _filehdr.flags;
    header->recSec = _filehdr.recSec;
    header->eofOffset = _filehdr.eofOffset;
    header->recLen = _filehdr.recLen;
    header->l3Alloc = _filehdr.l3Alloc;
    memcpy (header->name, _filehdr.tiname, 10);
}

void DiskFile::fromTifiles (Tifiles *header)
{
    _filehdr.secCount = header->secCount;
    _filehdr.flags = header->flags;
    _filehdr.recSec = header->recSec;
    _filehdr.eofOffset = header->eofOffset;
    _filehdr.recLen = header->recLen;
    _filehdr.l3Alloc = header->l3Alloc;
    memcpy (_filehdr.tiname, header->name, 10);
}

void DiskFile::printInfo (FILE *out)
{
    fprintf (out, "%-10.10s", _filehdr.tiname);
    fprintf (out, " %6d", _dirSector);

    fprintf (out, " %6d", _length);

    fprintf (out, " %02X%-17.17s", _filehdr.flags,filesShowFlags (_filehdr.flags));
    fprintf (out, " %8d", be16toh(_filehdr.secCount));
    fprintf (out, " %8d", _filehdr.recSec * be16toh (_filehdr.secCount));
    fprintf (out, " %10d", _filehdr.eofOffset);
    fprintf (out, " %7d", le16toh (_filehdr.l3Alloc)); // NOTE : LE not BE ?
    fprintf (out, " %7d", _filehdr.recLen);

    for (int i = 0; i < _chainCount; i++)
        fprintf (out, "%s%2d=(%d-%d)", i!=0?",":"", i, _chains[i].start, _chains[i].end);

    fprintf (out, "\n");
}

int DiskFile::read (uint8_t *buff, int offset, int len)
{
    int total = 0;

    printf ("# req read %d bytes from inode %d, chains=%d\n", len, _inode, _chainCount);

    for (int i = 0; i < _chainCount; i++)
    {
        printf ("# start=%d end=%d\n", _chains[i].start, _chains[i].end);
        for (int j = _chains[i].start; j <= _chains[i].end; j++)
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

            _sectorMap->read (j, buff, offset, count);
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
    int secCount = be16toh (_filehdr.secCount);
    int total = 0;

    printf ("# write file inode %d, chains=%d\n", _inode, _chainCount);
    printf ("# writing %d bytes at offset %d\n", len, offset);

    bool extendFile = true;
    for (chain = 0; chain < _chainCount; chain++)
    {
        printf ("# chain=%d start=%d end=%d\n", chain, _chains[chain].start, _chains[chain].end);

        for (sector = _chains[chain].start; sector <= _chains[chain].end; sector++)
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

    assert (secCount != 0 || chain == _chainCount);

    /*  We have found the position in the chain to start writing, which may at
     *  the end of the file.  If offset is non zero, or if offset+len is less
     *  than the end of the sector, then we need to do a read-modify-write (NO!)*/
    while (len)
    {
        int count;

        if (secCount == 0)
        {
            /*  We have reached EOF.  Allocate a new sector */
            if ((sector = _sectorMap->findFree (FIRST_DATA_SECTOR)) == -1)
            {
                printf ("# disk full, can't write to file\n");
                break;
            }

            printf ("# Allocate new sector %d\n", sector);
            _sectorMap->alloc (sector, 1);
            secCount++;
            _filehdr.secCount = htobe16 (be16toh (_filehdr.secCount) + 1);

            /*  Is the new sector contiguous to the last sector in the last
             *  chain?  If so, then update the end of chain, otherwise create a
             *  new chain */
            if (chain > 0 && _chains[chain-1].end + 1 == sector)
            {
                printf ("# Append to existing chain %d\n", chain-1);
                _chains[chain-1].end = sector;
            }
            else
            {
                /*  TODO check if we run out of chains */
                printf ("# Allocate new chain %d\n", chain);
                _chains[chain].start =
                _chains[chain].end = sector;
                _chainCount++;
                _needsWrite = true;
                chain++;
            }
        }

        if (offset + len > DISK_BYTES_PER_SECTOR)
            count = DISK_BYTES_PER_SECTOR - offset;
        else
            count = len;

        total += _sectorMap->write (sector, buff, offset, count);

        offset = 0;
        len -= count;
        buff += count;

        /*  Advance to the next sector.  Check if it is still in the current
         *  chain */
        sector++;
        secCount--;

        if (len && secCount && sector > _chains[chain].end)
        {
            printf ("# not at eof, but end of chain, sector=%d end=%d\n",
                    sector, _chains[chain].end);
            chain++;
            sector = _chains[chain].start;
            printf ("# start of chaini %d sector=%d\n", chain, sector);
        }
    }

    if (newLength > _length)
    {
        _length = newLength;
        // filehdr.len = htobe16 (newLength);
        _filehdr.eofOffset = newLength % DISK_BYTES_PER_SECTOR;
    }

    encodeChain ();
    _needsWrite = true;
    // writeDirectory ();

    return total;
}

void DiskFile::readDirEnt ()
{
    _sectorMap->read (_dirSector, (uint8_t*) &_filehdr, 0, DISK_BYTES_PER_SECTOR);
    // TODO tmp until files is cpp
    char osname[100];
    filesTI2Linux (_filehdr.tiname, osname);
    _osname = osname;
    decodeChain ();

    _length = be16toh (_filehdr.secCount) * DISK_BYTES_PER_SECTOR;

    if (_filehdr.eofOffset != 0)
        _length -= (DISK_BYTES_PER_SECTOR - _filehdr.eofOffset);

    _needsWrite = false;
}

void DiskFile::sync()
{
    if (_needsWrite)
    {
        printf ("# write dirent sector %d\n", _dirSector);
        _sectorMap->write (_dirSector, (uint8_t*) &_filehdr, 0, DISK_BYTES_PER_SECTOR);
        _needsWrite = false;
    }
}

