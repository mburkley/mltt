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

    #if 0
    int codeLen = WORD (data[0], data[1]);
    int addrLineNumbersTop = WORD (data[2], data[3]);
    int addrLineNumbersEnd = WORD (data[4], data[5]);
    int addrProgramTop = WORD (data[6], data[7]);
    printf ("length=%d\n", length);
    printf ("\ncodeLen = %04X\n", codeLen);
    printf ("top of line numbers = %04X\n", addrLineNumbersTop);
    printf ("addrLineNumbersEnd = %04X\n", addrLineNumbersEnd);
    printf ("program top = %04X\n", addrProgramTop);

    printf ("line number table size %04X\n", addrLineNumbersTop - addrLineNumbersEnd);
    int lineCount = (addrLineNumbersTop - addrLineNumbersEnd + 1) / 4;
    #endif
    decodeBasicProgram (data, length);
}

int filesReadBinary (const char *name, uint8_t *data, int maxLength)
{
    FILE *fp;
    int programSize;

    if ((fp = fopen (name, "r")) == NULL)
    {
        printf ("Failed to open %s\n", name);
        return -1;
    }

    fseek (fp, 0, SEEK_END);
    int fileSize = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if (fileSize > maxLength)
    {
        printf ("Binary file too big\n");
        fclose (fp);
        return -1;
    }

    int count = fread (&tifiles, 1, sizeof (tifiles), fp);

    printf ("Read %d bytes\n", count);

    if (count < 128 || tifiles.marker != 0x07 ||
        strncmp (tifiles.ident, "TIFILES", 7))
    {
        printf ("TIFILES identifier not seen, assume binary\n");
        memcpy (data, &tifiles, count);
        programSize = fread (data+count, sizeof (uint8_t), fileSize-count, fp);
        programSize += count;
        printf ("read %d/%d\n", programSize, fileSize);
    }
    else
    {
        /*  Convert big endian to local */
        tifiles.secCount = be16toh (tifiles.secCount);
        tifiles.l3Alloc = be16toh (tifiles.l3Alloc);
        tifiles.extHeader = be16toh (tifiles.extHeader);

        printf ("TIFILES header found\n");
        printf ("secCount=%d\n", tifiles.secCount);
        printf ("eof=%d\n", tifiles.eof);
        programSize = tifiles.secCount * 256;

        if (tifiles.eof != 0)
            programSize -= (256 - tifiles.eof);
        
        printf ("flags=%s\n", filesShowFlags (tifiles.flags));
        count = fread (data, sizeof (uint8_t), programSize, fp);
        printf ("read %d/%d\n", count+128, fileSize);
    }

    return programSize;
}

