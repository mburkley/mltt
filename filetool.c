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
        printf ("Rec/sec:      %d\n", meta->header.recSec);
        printf ("eof-offset:   %d\n", meta->header.eofOffset);
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

static void addTifilesHeader (const char *infile, const char *outfile,
                              uint8_t *data, int size)
{
    FileMetaData meta;
    filesInitMeta (&meta, outfile, size, BYTES_PER_SECTOR, 0, true, false);

    printf ("Creating %s with a TIFILES header\n", outfile);
    char str[10];
    int len = getxattr (infile, "user.tifiles.flags", str, 10);
    if (len > 0)
    {
        str[len]=0;
        printf ("Adding flags from xattr\n");
        meta.header.flags = (atoi (str));
    }

    len = getxattr (infile, "user.tifiles.reclen", str, 10);
    if (len > 0)
    {
        str[len]=0;
        printf ("Adding reclen from xattr\n");
        meta.header.recLen = atoi (str);
    }

    len = getxattr (infile, "user.tifiles.reclen", str, 10);
    if (len > 0)
    {
        str[len]=0;
        printf ("Adding reclen from xattr\n");
        meta.header.recLen = atoi (str);
    }

    if (meta.header.recLen != 0)
        meta.header.recSec = BYTES_PER_SECTOR / meta.header.recLen;

    meta.header.eofOffset = len % BYTES_PER_SECTOR;
    filesWriteBinary (outfile, data, size, &meta, false);
}

static void removeTifilesHeader (const char *outfile,
                                 FileMetaData meta, uint8_t *data, int len)
{
    printf ("Writing %s without TIFILES header\n", outfile);
    filesWriteBinary (outfile, data, len, NULL, false);

    char str[10];
    sprintf (str, "%d", meta.header.flags);
    printf ("Adding xattr flags\n");
    setxattr (outfile, "user.tifiles.flags", str, strlen (str), 0);

    sprintf (str, "%d", meta.header.recLen);
    printf ("Adding xattr recLen\n");
    setxattr (outfile, "user.tifiles.reclen", str, strlen (str), 0);
}

int main (int argc, char *argv[])
{
    char c;
    bool convertBasic = false;
    bool convertVar = false;
    bool convertEa3 = false;
    bool doEncode = false;
    bool doDecode = false;
    bool debug = false;
    bool doTifiles = false;

    while ((c = getopt(argc, argv, "debv3t")) != -1)
    {
        switch (c)
        {
            case 'x' : debug = true; break;
            case 'e' : doEncode = true; break;
            case 'd' : doDecode = true; break;
            case 'b' : convertBasic = true; break;
            case 'v' : convertVar = true; break;
            case '3' : convertEa3 = true; break;
            case 't' : doTifiles = true; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nTIFILES reader / writer\n\n");
        printf ("usage: %s [-tbv3de] <in-file> [<out-file>]\n", argv[0]);
        printf ("\twhere <in-file> is a binary file with or without a TIFILES header\n");
        printf ("\tand <out-file> is the output file.  Default action is to decode.\n");
        printf ("\tIf <out-file> is not supplied then stdout is used (decode only).\n");
        printf ("\tProviding <in-file> with no other options prints file information.\n");
        printf ("\t\t-x=debug\n");
        printf ("\t\t-d=decode\n");
        printf ("\t\t-e=encode\n");
        printf ("\t\t-b=basic program\n");
        printf ("\t\t-v=var/disp file\n");
        printf ("\t\t-3=ea3 (under development)\n");
        printf ("\t\t-t=add TIFILES header to output file\n");
        return 1;
    }

    if (doTifiles && argc - optind < 2)
    {
        fprintf (stderr, "The TIFILES option is not available to stdout - please specify an output file\n");
        exit (1);
    }

    if (doEncode && argc - optind < 2)
    {
        fprintf (stderr, "Can't encode to stdout - please specify an output file\n");
        exit (1);
    }

    if (doTifiles && doDecode)
    {
        fprintf (stderr, "Can't add a TIFILES header when decoding to text.\n");
        exit (1);
    }

    char *infile = argv[optind];
    char *outfile = "";

    if (argc - optind >= 2)
        outfile = argv[optind + 1];

    printf ("input is '%s' and out is '%s'\n", infile, outfile);

    if (doEncode)
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

        int size = filesReadText (infile, &input, debug);

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
    else if (doDecode)
    {
        FileMetaData meta;
        uint8_t *input = NULL;
        char *output = NULL;

        int size = filesReadBinary (infile, &input, &meta, debug);

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
    }
    else if (outfile[0] != 0)
    {
        FileMetaData meta;
        uint8_t *input = NULL;

        int size = filesReadBinary (infile, &input, &meta, debug);

        // Add or remove tifiles hdr only
        if (meta.hasTifilesHeader && doTifiles)
        {
            fprintf (stderr, "Input file already has a TIFILES hdr - nothing to do?\n");
            exit (1);
        }
        if (!meta.hasTifilesHeader && !doTifiles)
        {
            fprintf (stderr, "Input file doesn't have a TIFILES hdr and none requested- nothing to do?\n");
            exit (1);
        }

        if (doTifiles)
            addTifilesHeader (infile, outfile, input, size);
        else
            removeTifilesHeader (outfile, meta, input, size);
    }
    else
    {
        FileMetaData meta;
        uint8_t *input = NULL;

        int size = filesReadBinary (infile, &input, &meta, debug);
        fileInfo (infile, &meta, size);
    }

    return 0;
}

