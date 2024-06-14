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
 *  Manages file formats.  Files can have a tifiles header or not.  On read, a
 *  test is performed to determine if a tifiles header is present or not.  If
 *  present, file name and options are extracted from the header.  Otherwise
 *  defaults are used.  When writing a meta data structure is optional.  If
 *  provided a tifiles header is written.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/xattr.h>

#include <iostream>

using namespace std;

#include "files.h"
#include "disk.h"
#include "types.h"

Files::Files (const char *name, bool hasTIfiles, bool verbose)
{
    _hasTifilesHeader = hasTIfiles;
    _osname = name;
    _data = nullptr;
    _allocLen = 0;
    _dataLen = 0;
    _verbose = verbose;
}

bool Files::setSize (int size)
{
    if (size > _allocLen)
    {
        cerr << "Can't set file length to " << size << 
                ", is is less than allocated length " << _allocLen << endl;
        return false;
    }

    _dataLen = size;
    return true;
}

bool Files::realloc (int length)
{
    if (_allocLen >= length)
        return true;

    uint8_t *p;
    if ((p = (uint8_t*) ::realloc (_data, length)) == NULL)
        return false;

    _data = p;
    _allocLen = length;
    return true;
}

char *Files::showFlags (uint8_t flags)
{
    static char str[26];

    sprintf (str, "(%s%s%s%s%s%s)", 
        (flags & FLAG_VAR) ? "VAR":"FIX",
        (flags & FLAG_EMU) ? "-EMU" : "",
        (flags & FLAG_MOD) ? "-MOD" : "",
        (flags & FLAG_WP) ? "-WP" : "",
        (flags & FLAG_BIN) ? "-BIN" : "",
        (flags & FLAG_PROG) ? "-PROG" : "-DATA");

    return str;
}

void Files::setRecLen (int recLen)
{
    _header.recLen = recLen;

    if (recLen > 0)
        _header.recSec = DISK_BYTES_PER_SECTOR / recLen;
    else
        _header.recSec = 0;

    _header.eofOffset = _dataLen % DISK_BYTES_PER_SECTOR;
}

/*  Initialise the TIFILES data structure */
void Files::initTifiles (const char *name, int length, int sectorSize, int recLen, bool program, bool readOnly)
{
    memcpy (_header.ident, "TIFILES", 7);
    _header.marker = 0x07;
    if (recLen > 0)
    {
        _header.flags |= FLAG_VAR;
        _header.recLen = recLen;
        _header.recSec = sectorSize / recLen;
    }
    else
    {
        _header.recLen = 0;
        _header.recSec = 0;
    }

    _header.flags = 0;

    if (program)
        _header.flags |= FLAG_PROG;

    if (readOnly)
        _header.flags |= FLAG_WP;

    _header.secCount = htobe16 ((length + sectorSize - 1) / sectorSize);
    _osname == name;
    Files::Linux2TI (string(name), _header.name);
}

int Files::read ()
{
    FILE *fp;

    if ((fp = fopen (_osname.c_str(), "r")) == NULL)
    {
        cerr << "Failed to open " << _osname << endl;
        return -1;
    }

    fseek (fp, 0, SEEK_END);
    int fileSize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if (_hasTifilesHeader)
    {
        int count = fread (&_header, 1, sizeof (Tifiles), fp);

        if (count != sizeof (Tifiles) || _header.marker != 0x07 ||
            strncmp (_header.ident, "TIFILES", 7))
        {
            /*  TIFILES header not seen, rewind to read data again */
            fseek (fp, 0, SEEK_SET);

            if (_verbose)
                fprintf (stderr, "No TIFILES header seen, using defaults\n");

            initTifiles (_osname.c_str(), fileSize, DISK_BYTES_PER_SECTOR, 0, false, false);
            _hasTifilesHeader = false;
        }
        else
        {
            /*  TIFILES header seen */
            fileSize -= sizeof (Tifiles);
            _hasTifilesHeader = true;

            if (_header.eofOffset != 0)
                fileSize -= (256 - _header.eofOffset);
            
            TI2Linux (_header.name, _osname);
            /*  Convert big endian to local */
            // _secCount = be16toh (_header.secCount);
            // _l3Alloc = be16toh (_header.l3Alloc);
            // _extHeader = be16toh (_header.extHeader);
            // fileSize = _secCount * 256;

            if (_verbose)
            {
                fprintf (stderr, "TIFILES header found\n");
                fprintf (stderr, "secCount=%d\n", be16toh (_header.secCount));
                fprintf (stderr, "eofOffset=%d\n", _header.eofOffset);
                fprintf (stderr, "flags=%s\n", showFlags (_header.flags));
            }
        }
    }

    if (!realloc (fileSize))
    {
        fprintf (stderr, "Failed to allocate %d bytes\n", fileSize);
        fclose (fp);
        return -1;
    }

    _dataLen = fread (_data, sizeof (uint8_t), fileSize, fp);

    if (_verbose)
        fprintf (stderr, "read %d\n", _dataLen);

    return _dataLen;
}

int Files::write ()
{
    FILE *fp;

    if ((fp = fopen (_osname.c_str(), "w")) == NULL)
    {
        cerr << "Failed to create " << _osname << endl;
        return -1;
    }

    if (_hasTifilesHeader)
    {
        // _header.secCount = be16toh (_secCount);
        // _header.l3Alloc = be16toh (_l3Alloc);
        // _header.extHeader = be16toh (_extHeader);
        Files::Linux2TI (_osname, _header.name);

        int count = fwrite (&_header, 1, sizeof (Tifiles), fp);

        if (_verbose)
            fprintf (stderr, "Wrote %d byte TIFILES header\n", count);
    }

    int count = fwrite (_data, sizeof (char), _dataLen, fp);

    if (_verbose)
        fprintf (stderr, "Wrote %d bytes\n", count);

    fclose (fp);

    return count;
}

void Files::Linux2TI (string lname, char tname[])
{
    unsigned i;

    for (i = 0; i < lname.length(); i++)
    {
        if (lname[i] == '.')
            tname[i] = '/';
        else
            tname[i] = toupper (lname[i]);
    }

    for (; i < 10; i++)
        tname[i] = ' ';

    // cout << "# L2TI "<<lname<<" => " << tname << endl;
}

void Files::TI2Linux (const char tname[], string& lname)
{
    lname = "";
    int i;

    for (i = 0; i < 10; i++)
    {
        if (tname[i] == ' ')
            break;

        if (tname[i] == '/')
            lname += '.';
        else
            lname += tname[i];
    }

    // cout << "# TI2L "<<tname<<" => " << lname << endl;
}

void Files::getxattr (bool show)
{
    char data[10];
    int len = ::getxattr(_osname.c_str(), "user.tifiles.flags", data, 10);
    if (len > 0)
    {
        data[len]=0;
        _header.flags = atoi (data);
        if (show)
        {
            printf ("\nExtended attributes were found with the following information:\n");
            printf ("Flags:        %02x %s\n", atoi (data), Files::showFlags (atoi (data)));
        }
    }

    len = ::getxattr(_osname.c_str(), "user.tifiles.reclen", data, 10);
    if (len > 0)
    {
        data[len]=0;
        _header.recLen = atoi (data);
        if (show)
            printf ("Rec-len:      %d\n", atoi (data));
    }
}

void Files::setxattr ()
{
    char str[10];
    sprintf (str, "%d", _header.flags);
    ::setxattr (_osname.c_str(), "user.tifiles.flags", str, strlen (str), 0);

    sprintf (str, "%d", _header.recLen);
    ::setxattr (_osname.c_str(), "user.tifiles.reclen", str, strlen (str), 0);
}

