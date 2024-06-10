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

#include "files.h"
#include "types.h"

char *filesShowFlags (uint8_t flags)
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

/*  Initialise a new file meta data structure */
void filesInitMeta (FileMetaData *meta, const char *name, int length, int sectorSize, int recLen, bool program, bool readOnly)
{
    // TODO eofOffset
    // TODO l3Alloc

    filesInitTifiles (&meta->header, name, length, sectorSize, recLen, program,
                      readOnly);
    meta->secCount = (length + sectorSize - 1) / sectorSize;
    strcpy (meta->osname, name);
    // meta->extHeader = be16toh (meta->header.extHeader);
}

/*  Initialise a new TIFILES data structure */
void filesInitTifiles (Tifiles *header, const char *name, int length, int sectorSize, int recLen, bool program, bool readOnly)
{
    memset (header, 0, sizeof (Tifiles));
    memcpy (header->ident, "TIFILES", 7);
    header->marker = 0x07;
    if (recLen > 0)
    {
        header->flags |= FLAG_VAR;
        header->recLen = recLen;
        header->recSec = sectorSize / recLen;
    }
    else
    {
        header->recLen = 0;
        header->recSec = 0;
    }

    header->flags = 0;

    if (program)
        header->flags |= FLAG_PROG;

    if (readOnly)
        header->flags |= FLAG_WP;

    filesLinux2TI (name, header->name);
}

int filesReadText (const char *name, char **data, bool verbose)
{
    FILE *fp;

    if ((fp = fopen (name, "r")) == NULL)
    {
        fprintf (stderr, "Failed to open %s\n", name);
        return -1;
    }

    fseek (fp, 0, SEEK_END);
    int fileSize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if ((*data = realloc (*data, fileSize)) == NULL)
    {
        fprintf (stderr, "Failed to allocate %d bytes\n", fileSize);
        fclose (fp);
        return -1;
    }

    int count = fread (*data, sizeof (char), fileSize, fp);

    if (verbose)
        fprintf (stderr, "Read %d bytes\n", count);

    return count;
}

int filesWriteText (const char *name, char *data, int length, bool verbose)
{
    FILE *fp;

    if ((fp = fopen (name, "w")) == NULL)
    {
        fprintf (stderr, "Failed to create %s\n", name);
        return -1;
    }

    int count = fwrite (data, sizeof (char), length, fp);

    if (verbose)
        fprintf (stderr, "Wrote %d bytes\n", count);

    fclose (fp);

    return count;
}

int filesReadBinary (const char *name, uint8_t **data, FileMetaData *meta, bool verbose)
{
    FILE *fp;
    Tifiles header;

    if ((fp = fopen (name, "r")) == NULL)
    {
        fprintf (stderr, "Failed to open %s\n", name);
        return -1;
    }

    fseek (fp, 0, SEEK_END);
    int fileSize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    int count = fread (&header, 1, sizeof (Tifiles), fp);

    if (verbose)
        fprintf (stderr, "Read %d bytes\n", count);

    if (count != sizeof (Tifiles) || header.marker != 0x07 ||
        strncmp (header.ident, "TIFILES", 7))
    {
        /*  TIFILES header not seen, rewind to read data again */
        fseek (fp, 0, SEEK_SET);

        if (verbose)
            fprintf (stderr, "No TIFILES header seen, using defaults\n");

        if (meta)
        {
            filesInitMeta (meta, name, fileSize, BYTES_PER_SECTOR, 0, false, false);
            meta->hasTifilesHeader = false;
        }
    }
    else
    {
        /*  TIFILES header seen */
        fileSize -= sizeof (Tifiles);
        meta->hasTifilesHeader = true;

        if (header.eofOffset != 0)
            fileSize -= (256 - header.eofOffset);
        
        if (meta)
        {
            memcpy (&meta->header, &header, sizeof (Tifiles));
            filesTI2Linux (header.name, meta->osname);
            /*  Convert big endian to local */
            meta->secCount = be16toh (header.secCount);
            meta->l3Alloc = be16toh (header.l3Alloc);
            meta->extHeader = be16toh (header.extHeader);
            // fileSize = meta->secCount * 256;

            if (verbose)
            {
                fprintf (stderr, "TIFILES header found\n");
                fprintf (stderr, "secCount=%d\n", meta->secCount);
                fprintf (stderr, "eofOffset=%d\n", header.eofOffset);
                fprintf (stderr, "flags=%s\n", filesShowFlags (header.flags));
            }
        }
    }

    if ((*data = realloc (*data, fileSize)) == NULL)
    {
        fprintf (stderr, "Failed to allocate %d bytes\n", fileSize);
        fclose (fp);
        return -1;
    }

    count = fread (*data, sizeof (uint8_t), fileSize, fp);

    if (verbose)
        fprintf (stderr, "read %d\n", fileSize);

    return fileSize;
}

int filesWriteBinary (const char *name, uint8_t *data, int length,
                      FileMetaData *meta, bool verbose)
{
    FILE *fp;

    if ((fp = fopen (name, "w")) == NULL)
    {
        fprintf (stderr, "Failed to create %s\n", name);
        return -1;
    }

    if (meta)
    {
        meta->header.secCount = be16toh (meta->secCount);
        meta->header.l3Alloc = be16toh (meta->l3Alloc);
        meta->header.extHeader = be16toh (meta->extHeader);
        filesLinux2TI (name, meta->header.name);

        int count = fwrite (&meta->header, 1, sizeof (Tifiles), fp);

        if (verbose)
            fprintf (stderr, "Wrote %d byte TIFILES header\n", count);
    }

    int count = fwrite (data, sizeof (char), length, fp);

    if (verbose)
        fprintf (stderr, "Wrote %d bytes\n", count);

    fclose (fp);

    return count;
}

void filesLinux2TI (const char *lname, char tname[])
{
    int i;
    int len = 10;

    for (i = 0; i < len; i++)
    {
        if (!lname[i])
            break;

        if (lname[i] == '.')
            tname[i] = '/';
        else
            tname[i] = toupper (lname[i]);
    }

    for (; i < 10; i++)
        tname[i] = ' ';
}

void filesTI2Linux (const char *tname, char *lname)
{
    int i;

    for (i = 0; i < 10; i++)
    {
        if (tname[i] == ' ')
            break;

        if (tname[i] == '/')
            lname[i] = '.';
        else
            lname[i] = tname[i];
    }

    lname[i] = 0;
}

