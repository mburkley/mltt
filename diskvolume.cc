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
    bool insert = false;

    /*  Find where alphabetically the new file should go.  Use the TI names for
     *  the comparison as the order must be as TI would expect. */
    for (auto it = begin (_files); it != end (_files); it++)
    {
        if (strcmp (file->getTIName (), (*it)->getTIName ()) < 0)
        {
            printf ("# %s comes before %s\n", file->getTIName (), (*it)->getTIName ());
            _files.insert (it, file);
            insert = true;
            break;
        }
    }

    if (!insert)
    {
        printf ("# appending %s\n", file->getTIName ());
        _files.insert (end (_files), file);
    }

    _fileCount++;
    printf ("# file count now %d\n", _fileCount);
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
    _fileCount--;
}

void DiskVolume::readDirectory ()
{
    _sectors->read (DIR_HDR_SECTOR, (uint8_t*) _dirHdr, 0, DISK_BYTES_PER_SECTOR);

    for (int i = 0; i < DISK_BYTES_PER_SECTOR/2; i++)
    {
        int sector = _dirHdr[i][0] << 8 | _dirHdr[i][1];

        if (sector == 0)
            break;

        DiskFile *file = new DiskFile ("", _sectors, sector, FIRST_DATA_SECTOR, _lastInode++);
        file->readDirEnt ();
        addFileToList (file);
    }
}

void DiskVolume::updateDirectory ()
{
    memset (_dirHdr, 0, DISK_BYTES_PER_SECTOR);

    int entry = 0;

    for (auto it = begin (_files); it != end (_files); it++)
    {
        /*  Files that have been unlinked still exist but do not get an entry in
         *  the directory sector */
        if ((*it)->isUnlinked ())
            continue;

        printf ("# dir entry %d is sector %d\n", entry, (*it)->getDirSector());
        _dirHdr[entry][0] = (*it)->getDirSector () >> 8;
        _dirHdr[entry][1] = (*it)->getDirSector () & 0xff;

        (*it)->sync ();
        entry++;
    }

    _dirNeedsWrite = true;
}

void DiskVolume::format (DiskVolumeHeader *vol, const char *name, int
                         secPerTrk, int tracks, int sides, int density)
{
    Files::Linux2TI (name, vol->tiname);
    int sectors = tracks * secPerTrk * sides;
    vol->sectors = htobe16 (sectors);
    vol->secPerTrk = secPerTrk;
    memcpy (vol->dsk, "DSK", 3);
    vol->protect = ' ';
    vol->tracks = tracks;
    vol->sides = sides;
    vol->density = density;

    /*  Mark sector 0 and sector 1 as allocated */
    vol->bitmap[0] = 3;

    /*  Mark unavailable sectors as used */
    for (int i = sectors; i < 1440; i += 8)
        vol->bitmap[i/8] = 0xff;

    /*  Reserved area above bitmap must be set to 0xff */
    memset (vol->__reserved3, 0xff, sizeof (vol->__reserved3));
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
    DiskFile *file = new DiskFile (path, _sectors, 0, FIRST_DATA_SECTOR, _lastInode++);

    if (!file->create ())
    {
        delete file;
        return nullptr;
    }

    addFileToList (file);
    updateDirectory ();
    file->sync();
    sync();
    return file;
}

/*  Remove a file name from the directory and free its sectors */
bool DiskVolume::fileUnlink (const char *path)
{
    for (auto it : _files)
    // for (auto it = begin (_files); it != end (_files); it++)
    {
        printf ("# compare %s to inode %d=%s\n", path, it->getInode (),
        it->getOSName ().c_str());
        if (it->getOSName () == path)
        {
            printf ("# %s found %s\n", __func__, path);

            if (it->unlink ())
            {
                removeFileFromList (it);
                delete it;
            }

            updateDirectory ();
            sync ();
            return true;
        }
    }

    printf ("# Can't find %s to unlink\n", path);
    return false;
}

/*  Rename a file.  We need to rebuild the directory to keep it in order */
void DiskVolume::fileRename (DiskFile *file, const char *path)
{
    removeFileFromList (file);
    file->setOSName (path);
    addFileToList (file);
    updateDirectory ();
    file->sync();
    sync();
}

void DiskVolume::fileClose (DiskFile *file)
{
    if (file->close ())
    {
        removeFileFromList (file);
        delete file;
    }
}

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

    bool inuse = false;
    for (int i = 0; i < _sectorCount; i++)
    {
        bool bit = !_sectors->isFree (i);

        if (!inuse && bit)
        {
            fprintf (out,"[%d-", i);
            inuse = true;
        }
        else if (inuse && !bit)
        {
            fprintf (out,"%d]", i-1);
            inuse = false;
        }
    }

    if (inuse) fprintf (out,"%d]",_sectorCount-1);

    printf ("\n");
}

void DiskVolume::outputDirectory (FILE *out)
{
    fprintf (out, "Name       Sector Len    Flags               #Sectors #Records EOF-offset L3Alloc Rec-Len Sector chains\n");
    fprintf (out, "========== ====== ====== =================== ======== ======== ========== ======= ======= =======\n");

    for (auto f : _files)
        f->printInfo (out);
}

void DiskVolume::sync ()
{
    if (_dirNeedsWrite)
    {
        printf ("# Write dir\n");
        _sectors->write (DIR_HDR_SECTOR, (uint8_t*) _dirHdr, 0, DISK_BYTES_PER_SECTOR);
        _dirNeedsWrite = false;
    }

    _sectors->sync ();
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

    _sectors = new DiskSector (_volHdr.bitmap, (uint8_t*) &_volHdr, VOL_HDR_SECTOR, _fp);
    _sectors->read (0, (uint8_t*) &_volHdr, 0, sizeof (DiskVolumeHeader));
    Files::TI2Linux (_volHdr.tiname, _osname);

    _sectorCount = be16toh (_volHdr.sectors);
    _sectors->setSectorCount (_sectorCount);
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

