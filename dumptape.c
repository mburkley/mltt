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
 *  Emulate audio from the TMS9919 chip by generating samples and playing them
 *  through pulse audio.  Audio tones 1 thru 3 are straightforward but periodic
 *  and white noise are not as easy.  This guide has some useful info, but noise
 *  implementation is sitll not quite there : https://www.smspower.org/Development/SN76489
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sound.h"

FILE *cassetteFp;
static int cassetteSampleCount;

bool cassetteFileOpen (const char *name)
{
    WAV_FILE_HDR hdr;

    printf ("opening file for read\n");

    cassetteFp = fopen (name, "r");
    fread (&hdr, sizeof hdr, 1, cassetteFp);

    if (hdr.numChannels != 1 || hdr.bitsSample != 16 || hdr.sampleRate != 44100)
    {
        printf ("Unsupported audio file format\n");
        fclose (cassetteFp);
        cassetteFp = NULL;
        return false;
    }

    cassetteSampleCount = hdr.dataSize / 2;
    return true;
}

static bool preamble = true;
static bool haveHeader;

static int headerBytes = 0;
static int recordBytes = 0;
static int recordCount;
static int recordIndex;

struct _header
{
    unsigned char mark;
    unsigned char size1;
    unsigned char size2;
}
header;

struct _record
{
    unsigned char sync[8];
    unsigned char mark;
    unsigned char data[64];
    unsigned char checksum;
}
record[2];

 void decodeRecords (int byte)
{
    if (!haveHeader)
    {
        ((unsigned char *)&header)[headerBytes++] = byte;
        if (headerBytes == 3)
        {
            printf ("Record count = %d\n", header.size1);
            haveHeader = true;
        }
        return;
    }

    ((unsigned char *)&record[recordIndex])[recordBytes++] = byte;
    printf("recbytes=%d ", recordBytes);
    if (recordBytes == 74)
    {
        printf("adv rec\n");
        recordIndex++;
        recordBytes = 0;
    }

    if (recordIndex == 2)
    {
        printf ("decoding\n");
        if (memcmp (&record[0], &record[1], 74) != 0)
            printf ("*** Record %d mismatch\n", recordCount);

        if (record[0].mark != 0xFF || record[0].sync[0] != 0)
            printf ("*** Record %d mislaigned\n", recordCount);

        printf ("\nRecord %d:", recordCount);

        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
                printf ("%02X ", record[0].data[i*8+j]);

            printf ("\n");
        }

        printf ("checksum=%02X\n", record[0].checksum);

        recordIndex = 0;
        recordCount++;
    }
}

static void decode (int bit)
{
    static int byte;
    static int bitCount;

    if (bit && preamble)
        preamble = false;

    if (preamble)
        return;

    byte <<= 1;
    byte |= bit;
    bitCount++;

    if (bitCount == 8)
    {
        printf (" %02X\n", byte);
        decodeRecords (byte);
        byte = 0;
        bitCount = 0;
    }
}

static void analyse (void)
{
    short sample;
    int changeCount = 0;
    int state = 0;
    int lastState = 0;
    int halfBit = 0;

    for (int i = 0; i < cassetteSampleCount; i++)
    {
        fread (&sample, 2, 1, cassetteFp);
        state = (sample > 0) ? 1 : 0;

        if (state == lastState)
        {
            changeCount++;
            continue;
        }

        int bit = (changeCount < 24);
        if (bit)
        {
            halfBit++;

            if (halfBit == 2)
            {
                printf ("1");
                decode (1);
                halfBit = 0;
            }
        }
        else
        {
            printf ("0");
            decode (0);
            halfBit = 0;
        }

        lastState = state;
        changeCount = 0;
    }
}

int main (int argc, char *argv[])
{
    const char *name;

    if (argc < 2)
        name = "cassette.cassette";
    else
        name = argv[1];

    if (!cassetteFileOpen (name))
        return 0;

    analyse ();

    fclose (cassetteFp);

    return 0;
}
