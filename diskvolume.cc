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
 *  Implements Disk volume
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "files.h"
#include "diskvolume.h"

#define FIRST_INODE 100

DiskVolume::DiskVolume ()
{
    // _sectors = new DiskSector (_volHdr.bitmap);
    _sectors = nullptr;
    _osname = "";
    _fileCount = 0;
    _sectorCount = 0;
    _sectorsFree = 0;
    _fp = nullptr;
    _lastInode = FIRST_INODE;
}

/*  Insert a new file name into the directory maintaining alphabetic order.  If
 *  path is NULL, then add it to the end of the list */
void DiskVolume::addFileToList (DiskFile *file)
{
    // DiskFile *file = new DiskFile (lastInode++);

    bool insert = false;

    /*  Find where alphabetically the new file should go.  Use the TI names for
     *  the comparison as the order must be as TI would expect. */
    for (auto it = begin (_files); it != end (_files); it++)
    {
        if (strcmp (file->getTIName (), (*it)->getTIName ()) < 0)
        {
            printf ("# %s comes before %s; break;\n", file->getTIName (),
            (*it)->getTIName ());
            _files.insert (it, file);
            insert = true;
            break;
        }
    }

    if (!insert)
        _files.insert (end (_files), file);

    _fileCount++;
}

void DiskVolume::removeFileFromList (DiskFile *file)
{
    /*  Remove the file from the file list vector */
    std::vector<DiskFile *> newFiles;
    // for (auto it = begin (_files); it != end (_files); ++it)
    for (auto f : _files)
    {
        if (f != file)
            newFiles.push_back (f);
    }

    _files = newFiles;
    // _files.remove (_files.begin(), _files.end(), file);
    // _sectors.setDirUpdate ();
    _fileCount--;
    // sync ();
    delete file;
}

void DiskVolume::readDirectory ()
{
    _sectors->readDirSector();

    for (int i = 0; i < DISK_BYTES_PER_SECTOR/2; i++)
    {
        int sector = _dirHdr[i][0] << 8 | _dirHdr[i][1];

        if (sector == 0)
            break;

        DiskFile *file = new DiskFile ("", _sectors, sector, _lastInode++);
        file->readDirEnt ();
        addFileToList (file);
    }
}

void DiskVolume::updateDirectory ()
{
    // uint8_t data[DISK_BYTES_PER_SECTOR/2][2];
    // memset (data, 0, DISK_BYTES_PER_SECTOR);
    memset (_dirHdr, 0, DISK_BYTES_PER_SECTOR);

    int entry = 0;

    for (auto it = begin (_files); it != end (_files); it++)
    {
        /*  Files that have been unlinked still exist but do not get an entry in
         *  the directory sector */
        if ((*it)->isUnlinked ())
            continue;

        _dirHdr[entry][0] = (*it)->getDirSector () >> 8;
        _dirHdr[entry][1] = (*it)->getDirSector () & 0xff;

        (*it)->sync ();
        entry++;
    }

    _sectors->setDirUpdate ();
}

void DiskVolume::encodeVolumeHeader (DiskVolumeHeader *vol, const char *name, int
                            secPerTrk, int tracks, int sides, int density)
{
    filesLinux2TI (name, vol->tiname);
    // v->sectors = htons (SECTORS_PER_DISK);
    int sectors = tracks * secPerTrk * sides;
    vol->sectors = htobe16 (sectors);
    vol->secPerTrk = secPerTrk;
    memcpy (vol->dsk, "DSK", 3);
    vol->protect = ' ';
    vol->tracks = tracks;
    vol->sides = sides;
    vol->density = density;
}

DiskFile *DiskVolume::fileAccess (const char *path, int mode)
{
    // TODO check mode

    for (auto it = begin (_files); it != end (_files); it++)
    {
        printf ("compare %s %s\n", path, (*it)->getOSName ().c_str());
        if ((*it)->getOSName () == path)
        {
            printf ("# %s %s inode=%d\n", __func__, path, (*it)->getInode ());
            return *it;
        }
    }

    return nullptr;
}

DiskFile *DiskVolume::fileOpen (const char *path, int mode)
{
    DiskFile *file = fileAccess (path, mode);

    if (file)
    {
        file->incRefCount ();
        return file;
    }

    return nullptr;
}

DiskFile *DiskVolume::fileCreate (const char *path)
{
    DiskFile *file = new DiskFile (path, _sectors, 0, _lastInode++);
    file->create ();
    addFileToList (file);
    updateDirectory ();
    _sectors->setDirUpdate ();
    _sectors->sync();

    return file;
}

/*  Remove a file name from the directory and free its sectors */
bool DiskVolume::fileUnlink (const char *path)
{
    // uint8_t data[DISK_BYTES_PER_SECTOR/2][2];

    // fseek (_fp, DISK_BYTES_PER_SECTOR * 1, SEEK_SET);
    // fread (&data, 1, sizeof (data), fp);

    for (auto it : _files)
    // for (auto it = begin (_files); it != end (_files); it++)
    {
        printf ("# compare %s to inode %d=%s\n", path, it->getInode (),
        it->getOsName ().c_str());
        if (it->getOsName () == path)
        {
            printf ("# %s found %s\n", __func__, path);
            it->unlink ();
            updateDirectory ();
            _sectors->sync ();
            return true;
        }
    }
    // fseek (fp, DISK_BYTES_PER_SECTOR * 1, SEEK_SET);
    // fread (&data, 1, sizeof (data), fp);

    printf ("# Can't find %s to unlink\n", path);
    return false;
}

#if 0
void DiskVolume::setFileFlags (DiskFile& file, int flags)
{
    file.setFlags (flags);
    writeDirectory ();
}

void DiskVolume::setFileRecLen (DiskFile& file, int recLen)
{
    file.setRecLen (recLen);
    writeDirectory ();
}
#endif

void DiskVolume::outputHeader (FILE *out)
{
    fprintf (out,"Vol-Label='%-10.10s'", _volHdr.tiname);
    fprintf (out,", sectors=%d", ntohs (_volHdr.sectors));
    fprintf (out,", sec/trk:%d", _volHdr.secPerTrk);
    fprintf (out,", DSR:'%-3.3s'", _volHdr.dsk);
    fprintf (out,", Protect:'%c'", _volHdr.protect);
    fprintf (out,", tracks:%d", _volHdr.tracks);
    fprintf (out,", sides:%d", _volHdr.sides);
    fprintf (out,", density:%02X", _volHdr.density);

    if (_volHdr.date[0])
        fprintf (out,", year:%-8.8s", _volHdr.date);

    fprintf (out,"\nSectors allocated " );

    #if 1
    // TODO move the bitmap size calc out of output func
    int max = 0x64;
    if (_volHdr.sides==2)
    {
        if (_volHdr.density == 1)
            max = 0x91;
        else
            max = 0xeb;
    }

    bool inuse = false;
    for (int i = 0; i <= max - 0x38; i++)
    {
        uint8_t map=_volHdr.bitmap[i];

        for (int j = 0; j < 8; j++)
        {
            bool bit = (map & (1<<j));

            if (!inuse && bit)
            {
                fprintf (out,"[%d-", i*8+j);
                inuse = true;
            }
            else if (inuse && !bit)
            {
                fprintf (out,"%d]", i*8+j-1);
                inuse = false;
            }
        }
    }
    if (inuse) fprintf (out,"%d]",(max-0x38+1)*8);
    #endif
    printf ("\n");
}

void DiskVolume::outputDirectory (FILE *out)
{
    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sector chains\n");
    fprintf (out, "========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");

    for (auto f : _files)
    // for (DiskFile *file = firstFile; file != NULL; file = file->next)
        f->printInfo (out);
}

bool DiskVolume::open (const char *name)
{
    assert (_fp == nullptr);
    assert (_sectors == nullptr);

    if ((_fp = fopen (name, "r+")) == NULL)
    {
        printf ("Can't open %s\n", name);
        return false;
    }

    // uint8_t *zap = (uint8_t*) &(_dirHdr[0][0]);
    _sectors = new DiskSector ((uint8_t*)&_volHdr, (uint8_t*) _dirHdr, _volHdr.bitmap, _fp);
    _sectors->read (0, (uint8_t*) &_volHdr, 0, sizeof (DiskVolumeHeader));
    // TODO tmp
    char osname[100];
    filesTI2Linux (_volHdr.tiname, osname);
    _osname = osname;

    _sectorCount = be16toh (_volHdr.sectors);
    _sectorsFree = 0;

    for (int i = 0; i < _sectorCount; i++)
    {
        if (_sectors->isFree (i))
            _sectorsFree++;
    }

    printf ("# Volume %s has %d sectors (%d free)\n", _osname.c_str(),
            _sectorCount, _sectorsFree);
    readDirectory ();

    return true;
}

void DiskVolume::close ()
{
    fclose (_fp);
    _fp = nullptr;
    delete _sectors;
    _sectors = nullptr;
}

