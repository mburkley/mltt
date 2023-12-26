/*
 * Copyright (c) 2004-2023 Mark Burkley.
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
 *  Manages file formats
 */

#include <string.h>
#include <stdio.h>

#include "types.h"
#include "decodebasic.h"

static struct
{
    uint8_t marker;
    char ident[7];
    // 0x08
    int16_t secCount;
    uint8_t flags;
    uint8_t recSec;
    uint8_t eof;
    uint8_t recLen;
    int16_t l3Alloc;
    // 0x10
    char name[10];
    uint8_t mxt;
    uint8_t _reserved1;
    uint16_t extHeader;
    uint8_t timeCreate[4];
    uint8_t timeUpdate[4];
    uint8_t _reserved2[2];
    // 0x28
    uint8_t _reserved3[0x58];
}
tifiles;

char *filesShowFlags (uint8_t flags)
{
    static char str[24];

    sprintf (str, "%s%s%s%s%s%s", 
        (flags & 0x80) ? "VAR":"FIX",
        (flags & 0x20) ? "-EMU" : "",
        (flags & 0x10) ? "-MOD" : "",
        (flags & 0x08) ? "-WP" : "",
        (flags & 0x02) ? "-BIN" : "-ASC",
        (flags & 0x01) ? "-PROG" : "-DATA");

    return str;
}

void filesReadProgram (FILE *fp, uint8_t *data, int length)
{
    decodeBasicProgram (data, length);
}

int filesReadBinary (const char *name, uint8_t *data, int maxLength, bool verbose)
{
    FILE *fp;
    int programSize;

    if ((fp = fopen (name, "r")) == NULL)
    {
        fprintf (stderr, "Failed to open %s\n", name);
        return -1;
    }

    fseek (fp, 0, SEEK_END);
    int fileSize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if (fileSize > maxLength)
    {
        fprintf (stderr, "Binary file too big\n");
        fclose (fp);
        return -1;
    }

    int count = fread (&tifiles, 1, sizeof (tifiles), fp);

    if (verbose)
        fprintf (stderr, "Read %d bytes\n", count);

    if (count < 128 || tifiles.marker != 0x07 ||
        strncmp (tifiles.ident, "TIFILES", 7))
    {
        memcpy (data, &tifiles, count);
        programSize = fread (data+count, sizeof (uint8_t), fileSize-count, fp);
        programSize += count;

        if (verbose)
        {
            fprintf (stderr, "TIFILES identifier not seen, assume binary\n");
            fprintf (stderr, "read %d/%d\n", programSize, fileSize);
        }
    }
    else
    {
        /*  Convert big endian to local */
        tifiles.secCount = be16toh (tifiles.secCount);
        tifiles.l3Alloc = be16toh (tifiles.l3Alloc);
        tifiles.extHeader = be16toh (tifiles.extHeader);

        if (verbose)
        {
            fprintf (stderr, "TIFILES header found\n");
            fprintf (stderr, "secCount=%d\n", tifiles.secCount);
            fprintf (stderr, "eof=%d\n", tifiles.eof);
        }

        programSize = tifiles.secCount * 256;

        if (tifiles.eof != 0)
            programSize -= (256 - tifiles.eof);
        
        if (verbose)
            fprintf (stderr, "flags=%s\n", filesShowFlags (tifiles.flags));

        count = fread (data, sizeof (uint8_t), programSize, fp);

        if (verbose)
            fprintf (stderr, "read %d/%d\n", count+128, fileSize);
    }

    return programSize;
}

