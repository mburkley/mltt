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
 *  Creates a cassette audio file from a bin file.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "types.h"
#include "sound.h"
#include "parse.h"
#include "decodebasic.h"

int main (int argc, char *argv[])
{
    #if 0
    while (1)
    {
        if (argc >= 2 && !strcmp (argv[1], "-d"))
        {
            showContents = true;
            parseRemoveArg (&argc, argv);
            continue;
        }

        if (argc >= 2 && !strcmp (argv[1], "-b"))
        {
            showBasic = true;
            parseRemoveArg (&argc, argv);
            continue;
        }

        if (argc >= 2 && !strcmp (argv[1], "-r"))
        {
            showRaw = true;
            parseRemoveArg (&argc, argv);
            continue;
        }

        break;
    }
    #endif

    if (argc < 3)
    {
        printf ("usage: %s <bin-file> <wav-file>\n", argv[0]);
        // printf ("\t where -d = dump HEX, -b = decode basic, -r = raw bit dump\n");
        exit(1);
    }

    soundWavFileOpenWrite (argv[2]);

    if (!binFileRead (argv[1]))
        return 0;

    outputWav ();
    soundWavFileClose ();

    printf ("Wrote %d blocks in %d records\n", blockCount, recordCount);

    return 0;
}

