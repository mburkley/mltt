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
// #include <arpa/inet.h>

#include <iostream>

using namespace std;

#include "types.h"
#include "disk.h"
#include "parse.h"
#include "files.h"
#include "tibasic.h"
#include "file_disvar.h"

static void fileInfo (Files& file, const char *name, int size)
{
    printf ("File '%s' is %d bytes long\n", name, size);

    if (file.hasTIfiles ())
    {
        printf ("\nTIFILES header found with the following information:\n");
        printf ("TI file name: '%-10.10s'\n", file.getTiname ());
        printf ("Flags:        %02x %s\n", file.getFlags (), Files::showFlags (file.getFlags ()));
        printf ("Sectors:      %d\n", be16toh (file.getSecCount ()));
        printf ("Rec-len:      %d\n", file.getRecLen ());
        printf ("Rec/sec:      %d\n", file.getRecSec ());
        printf ("eof-offset:   %d\n", file.getEofOffset ());
    }

    file.getxattr (true);
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

    while ((c = getopt(argc, argv, "dexbv3t")) != -1)
    {
        switch (c)
        {
            case 'd' : debug = true; break;
            case 'e' : doEncode = true; break;
            case 'x' : doDecode = true; break;
            case 'b' : convertBasic = true; break;
            case 'v' : convertVar = true; break;
            case '3' : convertEa3 = true; break;
            case 't' : doTifiles = true; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nTIFILES reader / writer version " VERSION "\n\n");
        printf ("usage: %s [-tbv3de] <in-file> [<out-file>]\n", argv[0]);
        printf ("\twhere <in-file> is a binary file with or without a TIFILES header\n");
        printf ("\tand <out-file> is the output file.  If <out-file> is not supplied\n");
        printf ("\tthen stdout is used (decode only).\n");
        printf ("\tProviding <in-file> with no other options prints file information.\n");
        printf ("\t\t-d=debug\n");
        printf ("\t\t-x=decode\n");
        printf ("\t\t-e=encode\n");
        printf ("\t\t-b=basic program\n");
        printf ("\t\t-v=var/disp file\n");
        printf ("\t\t-3=ea3 (under development)\n");
        printf ("\t\t-t=add TIFILES header to output file\n");
        return 1;
    }

    if (doTifiles && argc - optind < 2)
    {
        cerr << "The TIFILES option is not available to stdout - please specify an output file" << endl;
        exit (1);
    }

    if (doEncode && argc - optind < 2)
    {
        cerr << "Can't encode to stdout - please specify an output file" << endl;
        exit (1);
    }

    if (doTifiles && doDecode)
    {
        cerr << "Can't add a TIFILES header when decoding to text." << endl;
        exit (1);
    }

    char *infile = argv[optind];
    const char *outfile = "";

    if (argc - optind >= 2)
        outfile = argv[optind + 1];

    // cout << "input is '" << infile << "' and out is '" << outfile << "'" << endl;

    if (doEncode)
    {
        Files input (infile, false, debug);
        Files output (outfile, doTifiles, debug);

        if (!convertBasic && !convertVar && !convertEa3)
        {
            cerr << "Please specify a conversion, -b, -v or -3" << endl;
            exit (1);
        }

        int size = input.read ();

        /*  Allocate a buffer to receive the tokenised output which is 50% bigger
         *  than the source code input */
        output.realloc (size * 1.5);

        if (convertBasic)
        {
            size = encodeBasicProgram ((char*) input.getData(), input.getSize(),
                                       output.getData(), debug);

            output.setSize (size);

            if (doTifiles)
                output.initTifiles (outfile, size, DISK_BYTES_PER_SECTOR, 0, true, false);

            output.write ();
            return 0;
        }
        else if (convertVar)
        {
            size = encodeDisVar ((char*) input.getData(), input.getSize(), output.getData());

            if (doTifiles)
                output.initTifiles (outfile, size, DISK_BYTES_PER_SECTOR, 80, false, false);

            // cout << "size=" << size << endl;
            output.write ();
        }
    }
    else if (doDecode)
    {
        Files input (infile, true, debug);
        Files output (outfile, doTifiles, debug);

        int size = input.read ();

        /*  Allocate a buffer to receive the tokenised output which is 50% bigger
         *  than the source code input */
        output.realloc (size * 1.5);

        if (convertBasic)
        {
            size = decodeBasicProgram  (input.getData(), input.getSize(),
                                        (char*) output.getData(), debug);
            // cerr << "size=" << size << endl;

            if (outfile[0])
                output.write ();
            else
                printf ("%-*.*s", size, size, output.getData ());
            return 0;
        }
        else if (convertVar)
        {
            size = decodeDisVar (input.getData(), input.getSize(), (char*) output.getData());
            // cerr << "size=" << size << endl;

            if (outfile[0])
                output.write ();
            else
                printf ("%-*.*s", size, size, output.getData ());
        }
        else
        {
            cerr << "Please specify a conversion to decode" << endl;
            exit (1);
        }
    }
    else if (outfile[0] != 0)
    {
        Files input (infile, true, debug); // input might have Tifiles header
        Files output (outfile, doTifiles, debug);

        int size = input.read ();

        // Add or remove tifiles hdr only
        if (input.hasTIfiles () && doTifiles)
        {
            cerr << "Input file already has a TIFILES hdr - nothing to do?" << endl;
            exit (1);
        }
        if (!input.hasTIfiles () && !doTifiles)
        {
            cerr << "Input file doesn't have a TIFILES hdr and none requested- nothing to do?" << endl;
            exit (1);
        }

        output.setData (input.getData(), size);

        if (doTifiles)
        {
            printf ("Creating %s with a TIFILES header\n", outfile);
            output.initTifiles (infile, size, DISK_BYTES_PER_SECTOR, 0, false, false);
            output.getxattr (false);
            output.write ();
        }
        else
        {
            printf ("Writing %s without TIFILES header\n", outfile);
            output.write ();
            output.setxattr ();
        }
    }
    else
    {
        Files input (infile, true, debug); // input might have Tifiles header
        int size = input.read ();
        fileInfo (input, infile, size);
    }

    return 0;
}

