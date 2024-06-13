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
#include "diskvolume.h"

static void extractFile (DiskVolume& volume, const char *name)
{
    char linuxFile[100];
    filesTI2Linux (name, linuxFile);

    DiskFile *file;
    if ((file = volume.fileAccess (linuxFile, 0)) == nullptr)
    {
        fprintf (stderr, "Can't access disk file %s\n", linuxFile);
        exit (1);
    }

    int len = file->getLength ();

    uint8_t *data = (uint8_t*) malloc (len);
    file->read (data, 0, len);

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
    DiskVolume volume;
    char c;
    bool extract = false;
    bool format = false;
    // bool add = false;
    // bool remove = false;
    const char *file;
    const char *volName = "BLANK";
    int tracks = 40;
    int secPerTrk = 9;
    int sides = 1;

    while ((c = getopt(argc, argv, "x:r:a:ft:n:p:s:")) != -1)
    {
        /*  Extract works but add and remove do not.  Not sure there is any
         *  point in implementing now when fuse is available */
        switch (c)
        {
            case 'x' : extract = true; file=optarg; break;
            case 'f' : format = true; break;
            case 't':  tracks = atoi (optarg); break;
            case 'n':  volName = optarg; break;
            case 'p':  secPerTrk = atoi (optarg); break;
            case 's':  sides = atoi (optarg); break;
            // case 'r' : remove = true; file=optarg; break;
            // case 'a' : add = true; file=optarg; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nSector dump disk file read tool\n\n");
                // "usage: %s [-x <extract-file>] [-r <remove-file>] [-a <add-file>] <dsk-file>\n\n", argv[0]);
        printf ("usage: %s [-x <extract-file>] "
                "[-f -n <name> -t <tracks> -p <sectors-per-track> -s <sides>]] <dsk-file>\n", argv[0]);
        printf ("\twhere -f \"formats\" a new disk image, defaults are 720, 40, 9, 1\n\n");
        return 1;
    }

    if (format)
    {
        struct _data
        {
            DiskVolumeHeader vol;
            uint8_t blank[0];
        }
        *data;

        int sectors = tracks * secPerTrk * sides;
        int size = sectors * DISK_BYTES_PER_SECTOR;

        if (sectors == 0)
        {
            fprintf (stderr, "Can't format a disk with zero sectors\n");
            exit (1);
        }

        data = (struct _data *) calloc (size, 1);
        // diskVolume.encodeHeader (&data->vol, volName, secPerTrk, tracks, sides, 1);

        /*  Mark sector 0 and sector 1 as allocated */
        data->vol.bitmap[0] = 3;

        size = filesWriteBinary (argv[optind], (uint8_t*) data, size, NULL, false);
        printf ("Wrote %d bytes to create volume '%s' with %d sectors\n", size, volName, sectors);
        return 0;
    }


    if (!volume.open (argv[optind]))
    {
        fprintf (stderr, "Can't open %s\n", argv[optind]);
        exit (1);
    }

    if (extract)
        extractFile (volume, file);
    #if 0
    else if (add)
        ;
    else if (remove)
        ;
    #endif
    else
    {
        volume.outputHeader (stdout);
        volume.outputDirectory (stdout);
    }

    volume.close ();

    return 0;
}
