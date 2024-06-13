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
 *  Compile (tokenise) or decompile a basic program
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "types.h"
#include "disk.h"
#include "parse.h"
#include "files.h"
#include "tibasic.h"

int main (int argc, char *argv[])
{
    uint8_t *binary = NULL;
    char *text = NULL;
    char c;
    bool decompile = false;
    bool debug = false;
    bool header = true;

    while ((c = getopt(argc, argv, "drn")) != -1)
    {
        switch (c)
        {
            case 'd' : debug = true; break;
            case 'r' : decompile = true; break;
            case 'n' : header = false; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    /*  Decompile with no output file defaults to stdout.  Compile creates
     *  binary output so an output file name must be specified */
    if (argc - optind < 1 || (!decompile && argc - optind < 2))
    {
        printf ("\nTI basic compiler/decompiler\n\n");
        printf ("usage: %s [-drn] <input-file> [<output-file>]\n", argv[0]);
        printf ("\t where -d=debug, -r=reverse (decompile), -n=no tifiles header\n");
        printf ("\t<input-file> is a text file for compile, or tifiles or binary file for decompile\n\n");
 
        return 1;
    }

    if (decompile)
    {
        int size = filesReadBinary (argv[optind], &binary, NULL, true);
        size = decodeBasicProgram  (binary, size, &text, debug);

        if (argc - optind > 1)
            filesWriteText (argv[optind+1], text, size, true);
        else
            printf ("%s\n", text);
    }
    else
    {
        int size = filesReadText (argv[optind], &text, true);
        size = encodeBasicProgram  (text, size, &binary, debug);

        if (header)
        {
            FileMetaData meta;
            filesInitMeta (&meta, argv[optind+1], size, DISK_BYTES_PER_SECTOR, 0, true, false);
            filesWriteBinary (argv[optind+1], binary, size, &meta, true);
        }
        else
            filesWriteBinary (argv[optind+1], binary, size, NULL, true);
    }

    return 0;
}

