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
 *  Tool to manipulate files.  Add or remove TIFILES header, encode/decode
 *  basic, etc.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/xattr.h>

#include "types.h"
#include "parse.h"
#include "files.h"
#include "tibasic.h"
#include "file_disvar.h"

static void fileInfo (const char *name, FileMetaData *meta, int size)
{
    printf ("File '%s' is %d bytes long\n", name, size);

    if (meta->hasTifilesHeader)
    {
        printf ("\nTIFILES header found with the following information:\n");
        printf ("TI file name: '%-10.10s'\n", meta->header.name);
        printf ("Flags:        %02x %s\n", meta->header.flags, filesShowFlags (meta->header.flags));
        printf ("Sectors:      %d\n", meta->secCount);
        printf ("Rec-len:      %d\n", meta->header.recLen);
    }

    char data[10];
    int len = getxattr(name, "user.tifiles.flags", data, 10);
    if (len > 0)
    {
        data[len]=0;
        printf ("\nExtended attributes were found with the following information:\n");
        printf ("Flags:        %02x %s\n", atoi (data), filesShowFlags (atoi (data)));
    }

    len = getxattr(name, "user.tifiles.reclen", data, 10);
    if (len > 0)
    {
        data[len]=0;
        printf ("Rec-len:      %d\n", atoi (data));
    }
}

int main (int argc, char *argv[])
{
    char c;
    char *outfile;
    bool convertBasic = false;
    bool convertVar = false;
    bool convertEa3 = false;
    bool doCreate = false;
    bool debug = false;
    bool doTifiles = false;

    while ((c = getopt(argc, argv, "tbv3dc:")) != -1)
    {
        switch (c)
        {
            case 'b' : convertBasic = true; break;
            case 'v' : convertVar = true; break;
            case '3' : convertEa3 = true; break;
            case 'd' : debug = true; break;
            case 't' : doTifiles = true; break;
            case 'c' : doCreate = true; outfile=optarg; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nTIFILES reader / writer\n\n");
        printf ("usage: %s [-tbv3d] [-c <out-file>] <in-file>\n", argv[0]);
        printf ("\twhere <in-file> is a binary file with or without a TIFILES header\n");
        printf ("\t\t-d=debug\n");
        printf ("\t\t-b=encode/decode basic program\n");
        printf ("\t\t-v=convert var/disp records\n");
        printf ("\t\t-3=convert contents of ea3 (under development)\n");
        printf ("\t\t-t=add TIFILES header to output file or ensure present on input\n");
        printf ("\t\t-c=create file from basic or var-text\n\n");
        return 1;
    }

    #if 0
    if (doTifiles && !doCreate)
    {
        fprintf (stderr, "The TIFILES option is only available when creating a file\n");
        exit (1);
    }
    #endif

    if (doCreate)
    {
        char *input = NULL;
        uint8_t *output = NULL;
        FileMetaData meta;
        FileMetaData *metap = NULL;

        if (!convertBasic && !convertVar && !convertEa3)
        {
            fprintf (stderr, "Please specify a conversion, -b, -v or -3\n");
            exit (1);
        }

        if (doTifiles)
            metap = &meta;

        int size = filesReadText (argv[optind], &input, debug);

        if (convertBasic)
        {
            size = encodeBasicProgram  (input, size, &output, debug);

            if (metap)
                filesInitMeta (metap, outfile, size, BYTES_PER_SECTOR, 0, true, false);

            filesWriteBinary (outfile, output, size, metap, debug);
            return 0;
        }
        else if (convertVar)
        {
            size = encodeDisVar (input, size, &output);

            if (metap)
                filesInitMeta (metap, outfile, size, BYTES_PER_SECTOR, 80, false, false);

            printf ("size=%d\n", size);
            filesWriteBinary (outfile, output, size, metap, debug);
        }
    }
    else
    {
        FileMetaData meta;
        uint8_t *input = NULL;
        char *output = NULL;

        int size = filesReadBinary (argv[optind], &input, &meta, debug);

        if (convertBasic)
        {
            size = decodeBasicProgram  (input, size, &output, debug);
            fprintf (stderr, "size=%d\n", size);
            printf ("%-*.*s", size, size, output);
            return 0;
        }
        else if (convertVar)
        {
            size = decodeDisVar (input, size, &output);
            fprintf (stderr, "size=%d\n", size);
            printf ("%-*.*s", size, size, output);
        }
        else
        {
            fileInfo (argv[optind], &meta, size);
        }
    }

    return 0;
}

