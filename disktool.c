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
 *  Dump the contents of a disk file
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "types.h"
#include "parse.h"
#include "files.h"
// #include "decodebasic.h"
#include "dskdata.h"

// static FILE *diskFp;
static bool showContents = false;
static bool showBasic = false;

int main (int argc, char *argv[])
{
    char c;

    while ((c = getopt(argc, argv, "db")) != -1)
    {
        switch (c)
        {
            case 'd' : showContents = true; break;
            case 'b' : showBasic = true; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nSector dump disk file read tool\n\n"
                "usage: %s [-d] [-b] <dsk-file>\n"
                "\t where -d=dump HEX, -b=decode basic\n\n", argv[0]);
        return 1;
    }

    DskInfo *info = dskOpenVolume (argv[optind]);
    dskOutputVolumeHeader (info, stdout);
    dskOutputDirectory (info, stdout);
    dskCloseVolume (info);

    return 0;
}
