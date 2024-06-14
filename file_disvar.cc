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

/* Handle var files */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "files.h"
#include "file_disvar.h"

int encodeDisVar (char *input, int len, uint8_t *output)
{
    char *end = input;
    uint8_t *outp = output;
    int spaceInSector = DISK_BYTES_PER_SECTOR;

    while (len--)
    {
        if (*end++ == '\n')
        {
            int reclen = end - input - 1;

            if (reclen > 80)
            {
                fprintf (stderr, "File contains a line longer than 80 characters\n");
                exit (1);
            }

            /*  If the current line won't fit into the remaining space in the
             *  current sector, then mark the end of data with 0xff, pad the
             *  space with zeroes and start the next sector.  Add 2 to the
             *  reclen when comparing to allow space for the length byte and the
             *  FF marker. */
            printf ("reclen=%d space=%d\n", reclen, spaceInSector);
            if (reclen + 2 > spaceInSector)
            {
                printf ("pad %d\n", spaceInSector-1);
                *outp++ = 0xff;
                // spaceInSector--;
                while (--spaceInSector > 0)
                    *outp++ = 0;
                spaceInSector = DISK_BYTES_PER_SECTOR;
            }

            *outp++ = reclen;
            memcpy (outp, input, reclen);
            outp += reclen;
            input = end;
            spaceInSector -= (reclen+1);
        }
    }

    return outp - output;
}

int decodeDisVar (uint8_t *input, int len, char *output)
{
    char *outp = output;

    int pos = 0;
    int sectorsRemain = (len + 1) / DISK_BYTES_PER_SECTOR;

    // printf ("remaining sectors %d\n", sectorsRemain);

    while (pos < len)
    {
        int reclen = input[pos];

        if (reclen == 0xff)
        {
            int sector = pos / DISK_BYTES_PER_SECTOR;
            pos = (sector + 1) * DISK_BYTES_PER_SECTOR;
            sectorsRemain--;
            // printf ("adv next, remain=%d\n", sectorsRemain);
            continue;
        }
        
        memcpy (outp, &input[pos+1], reclen);
        outp += reclen;
        *outp++ = '\n';
        pos += reclen + 1;
    }

    return outp - output;
}


