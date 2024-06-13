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
 *  Implements floppy disk sector dump file read and write.
 */

#include <stdio.h>
#include <string.h>
#include "trace.h"
#include "fddfile.h"
#include "fdd.h"

/*  Maintain static state even though we have multiple drives.  This is ok
 *  because only one drive can be selected at a time */
static FILE *diskFile;

static void fileSeek (int sector)
{
    ASSERT (diskFile != NULL, "file not open in seek");
    printf("XX seek %d\n", sector);
    fseek (diskFile, sector * DISK_BYTES_PER_SECTOR, SEEK_SET);
}

static void fileRead (unsigned char *buffer)
{
    ASSERT (diskFile != NULL, "file not open in read");
    int res = fread (buffer, 1, DISK_BYTES_PER_SECTOR, diskFile);
    printf("XX read %d\n", res);
}

static void fileWrite (unsigned char *buffer)
{
    ASSERT (diskFile != NULL, "file not open in write");
    /*  If the file was opened read only then write will fail quietly */
    int res = fwrite (buffer, 1, DISK_BYTES_PER_SECTOR, diskFile);
    printf("XX write %d\n", res);
}

static void fileSelect(const char *name, bool readOnly)
{
    const char *mode = readOnly ? "r" : "r+";

    ASSERT (diskFile == NULL, "already open");
    diskFile = fopen (name, mode);
    ASSERT (diskFile != NULL, "diskSelectDrive");
    printf ("Disk file %s selected\n", name);
}

static void fileDeselect(void)
{
    ASSERT (diskFile != NULL, "deselect not open");
    fclose (diskFile);
    diskFile = NULL;
}

void diskFileLoad (int drive, bool readOnly, const char *name)
{
    FILE *fp;
    fddHandler handler =
    {
        .seek = fileSeek,
        .read = fileRead,
        .write = fileWrite,
        .select = fileSelect,
        .deselect = fileDeselect,
        .readOnly = readOnly,
        // .name = ""
    };

    handler.name = strdup (name);

    /*  Test the file status by opening with as read only or as read with
     *  update.  If open read with update fails, create the file.  If that fails,
     *  halt */
    if (readOnly)
    {
        if ((fp = fopen (name, "r")) == NULL)
        {
            halt ("disk load");
        }
    }
    else if ((fp = fopen (name, "r+")) == NULL)
    {
        printf ("Creating new disk file '%s'\n", name);
        if ((fp = fopen (name, "w+")) == NULL)
            halt ("disk load");
    }

    fclose (fp);

    fddRegisterHandler (drive, &handler);
}

