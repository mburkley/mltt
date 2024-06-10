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
 *  Dump the contents of a sector dump disk file and optionally extract a file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "types.h"
#include "parse.h"
#include "files.h"
#include "tibasic.h"
#include "dskdata.h"

static void extractFile (DskInfo *info, const char *file)
{
    int index;

    char linuxFile[100];
    filesTI2Linux (file, linuxFile);

    if ((index = dskCheckFileAccess (info, linuxFile, 0)) < 0)
    {
        fprintf (stderr, "Can't access disk file %s\n", linuxFile);
        exit (1);
    }

    int len = dskFileLength (info, index);

    unsigned char *data = malloc (len);
    dskReadFile (info, index, data, 0, len);

    FILE *fp;
    if ((fp = fopen (linuxFile, "w")) == NULL)
    {
        fprintf (stderr, "Can't create %s\n", linuxFile);
        exit (1);
    }
    fwrite (data, 1, len, fp);
    fclose (fp);
    printf ("Extracted %s len %d\n", linuxFile, len);
    free (data);
}

int main (int argc, char *argv[])
{
    char c;
    bool extract = false;
    bool add = false;
    bool remove = false;
    const char *file;

    while ((c = getopt(argc, argv, "x:r:a:")) != -1)
    {
        switch (c)
        {
            case 'x' : extract = true; file=optarg; break;
            case 'r' : remove = true; file=optarg; break;
            case 'a' : add = true; file=optarg; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nSector dump disk file read tool\n\n"
                "usage: %s [-x <extract-file>] [-r <remove-file>] [-a <add-file>] <dsk-file>\n\n", argv[0]);
        return 1;
    }

    DskInfo *info = dskOpenVolume (argv[optind]);

    if (extract)
        extractFile (info, file);
    else if (add)
        ;
    else if (remove)
        ;
    else
    {
        dskOutputVolumeHeader (info, stdout);
        dskOutputDirectory (info, stdout);
    }

    dskCloseVolume (info);

    return 0;
}
