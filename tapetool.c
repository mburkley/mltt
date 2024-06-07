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
 *  Tool to read or create cassette audio files.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "types.h"
#include "wav.h"
#include "cassette.h"
#include "parse.h"
#include "files.h"
#include "tibasic.h"

static struct
{
    bool create;
    bool extract;
    bool verbose;
    bool basic;
    bool raw;
    bool wav;
}
options;

static bool errorsFound = false;
static bool errorsFixable = true;
static bool preambleSync = false;
static int preambleBitsExpected;

static bool haveHeader;

static int headerBytes = 0;
static int blockBytes = 0;
static int blockCount;
static int blockIndex;
static int recordCount;

struct _header
{
    uint8_t mark;
    uint8_t size1;
    uint8_t size2;
}
header;

struct _block
{
    // uint8_t sync[8];
    uint8_t mark;
    uint8_t data[64];
    uint8_t checksum;
}
block[2];

static uint8_t program[MAX_BINARY_SIZE];
static char basic[MAX_TEXT_SIZE];

static void decodeBlock (int byte)
{
    if (!haveHeader)
    {
        ((uint8_t *)&header)[headerBytes++] = byte;
        if (headerBytes == 3)
        {
            if (options.verbose) printf ("Reading %d blocks ...", header.size1);
            haveHeader = true;
            preambleSync = true;
            preambleBitsExpected = 60;
        }
        return;
    }

    ((uint8_t *)&block[blockIndex])[blockBytes++] = byte;
    // printf("recbytes=%d ", blockBytes);
    if (blockBytes == sizeof (struct _block))
    {
        // printf("adv rec\n");
        blockIndex++;
        blockBytes = 0;

        /*  Turn back on preamble synchronisatoin to sync with the next data
         *  block
         */
        preambleSync = true;
        preambleBitsExpected = 60;
    }


    /*  Do we have two complete blocks?  If so, compare them and calcualte
     *  checksums
     */
    if (blockIndex == 2)
    {
        // printf ("decoding\n");
        if (block[0].mark != 0xFF) // || block[0].sync[0] != 0)
        {
            fprintf (stderr, "*** Block %d misaligned\n", blockCount);
            errorsFound = true;
            errorsFixable = false;
        }

        if (memcmp (&block[0], &block[1], sizeof (struct _block)))
        {
            fprintf (stderr, "*** Block %d mismatch\n", blockCount);
            errorsFound = true;
        }

        if (options.verbose)
            printf ("\nBlock %d:\n", blockCount);

        int sum1 = 0;
        int sum2 = 0;

        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                uint8_t byte1 = block[0].data[i*8+j];
                uint8_t byte2 = block[1].data[i*8+j];

                if (options.verbose)
                {
                    /*  If bytes in both copies are not identical, print both */
                    if (byte1 != byte2)
                        printf ("[%2X != %02X] ", byte1, byte2);
                    else
                        printf ("%02X ", byte1);
                }

                sum1 += byte1;
                sum2 += byte2;
            }

            if (options.verbose)
            {
                for (int j = 0; j < 8; j++)
                {
                    uint8_t byte = block[0].data[i*8+j];

                    printf ("%c", (byte >= 32 && byte < 128) ? byte : '.');
                }

                printf ("\n");
            }
        }

        sum1 &= 0xff;
        sum2 &= 0xff;
        sum1 -= block[0].checksum;
        sum2 -= block[1].checksum;

        /*  At this point both sum1 and sum2 should be zero.  If either or
         *  both is non zero, print an error.  If only one is bad, copy from
         *  the good to the bad.
         */
        if (!sum1 && !sum2)
        {
            if (options.verbose)
                printf ("GOOD checksum=%02X\n", block[0].checksum);
        }
        else if (sum1 && sum2)
        {
            fprintf (stderr, "*** Block %d, both copies have BAD checksum\n", blockCount);
            errorsFound = true;
            errorsFixable = false;
        }
        else if (sum1)
        {
            fprintf (stderr, "*** Block %d, first copy has BAD checksum, using second copy\n", blockCount);
            memcpy (block[0].data, block[1].data, 64);
            errorsFound = true;
        }
        else if (sum2)
        {
            fprintf (stderr, "*** Block %d, second copy has BAD checksum, ignoring\n", blockCount);
            errorsFound = true;
        }

        memcpy (program + blockCount * 64, block[0].data, 64);
        blockIndex = 0;
        blockCount++;
    }
}

static void decodeBit (int bit)
{
    static int byte;
    static int bitCount;
    static int preambleCount;

    if (bit && preambleSync)
    {
        if (options.raw) printf ("preamble count=%d\n", preambleCount);
        /*  We expect the preamble to be 6144 bits at the start of the file and
         *  64 bits at the start of each block.  So if longer than 3072 bits we
         *  assume new record.
         */
        if (preambleCount > 3072)
        {
            recordCount++;

            if (haveHeader)
            {
                // printf ("*** New record\n");
                haveHeader = false;
                // blockCount =
                // blockIndex =
                // blockBytes =
                headerBytes = 0;
            }
        }

        /*  Reset the preamble bit count.  If we have seen at least 60 zero bits in a row we consider this to
         *  be a valid preamble and start reading data.
         */
        if (preambleCount > preambleBitsExpected)
            preambleSync = false;

        preambleCount = 0;
    }

    if (preambleSync)
    {
        preambleCount++;
        return;
    }

    byte <<= 1;
    byte |= bit;
    bitCount++;

    if (bitCount == 8)
    {
        if (options.raw)
            printf (" %02X (%d/%ld)\n", byte, blockBytes, sizeof (struct _block));
        decodeBlock (byte);
        byte = 0;
        bitCount = 0;
    }
}

static void inputBitWidth (int count)
{
    static int halfBit = 0;
    if (options.raw) printf("[%d]", count);
    /*  A peak for a 0 occurs at around 32 samples, for a 1 it is
     *  around 16 samples.  Select 24 as the cutoff to differentiate a 0
     *  from a 1.
     */
    int bit = (count < 24);
    if (bit)
    {
        halfBit++;

        if (halfBit == 2)
        {
            if (options.raw) printf ("1");

            decodeBit (1);
            halfBit = 0;
        }
    }
    else
    {
        if (options.raw) printf ("0");
        decodeBit (0);
        halfBit = 0;
    }
}

/*  Take data from a WAV file and carve it up into bits.  Due to some recorded
 *  files having DC offsets or mains hum elements, detecting zero crossings only
 *  was found to be unreliable.  Instead, maintain local min and max vars to
 *  track sine wave peaks.  Apply hysteresis to the detection so switch the bit
 *  from a zero to a one or vice versa once 50% of the peak is seen.  To apply
 *  gain control, the peak is diminished by 1% per sample, which is about 72%
 *  per bit (32 samples).  We look for 80% of peak which is a relative
 *  ampltitude of 0.5 before changing state. */
static void inputWav (wavState *wav)
{
    int16_t sample;
    int changeCount = 0;
    int state = 0;
    int lastState = 0;
    double localMin = 0;
    double localMax = 0;

    for (int i = 0; i < wavSampleCount (wav); i++)
    {
        localMax = 0.99 * localMax;  // 0.99 ^ 32 = 0.725
        localMin = 0.99 * localMin;
        sample = wavReadSample (wav);

        if (sample > localMax)
            localMax = sample;

        if (sample < localMin)
            localMin = sample;

        // Apply hysteresis
        if (state && sample > 0.8 * localMax)
            state = 0;

        if (!state && sample < 0.8 * localMin)
            state = 1;

        if (state == lastState)
        {
            changeCount++;
            continue;
        }

        // Found a potential bit, analyse it
        inputBitWidth (changeCount);
        lastState = state;
        changeCount = 0;
    }

    /*  Do a final call to process the width of the last sample */
    inputBitWidth (changeCount);
}

#define MAX_PROGRAM_SIZE    0x4000
#define BIT_DURATION        730000 // 730 usec = 1370 Hz

static int blockCount;
static int recordCount;
static int programSize;

// static uint8_t program[MAX_BINARY_SIZE];

static void outputByte (uint8_t byte)
{
    /*  Output bits from MSB thru LSB */
    for (int i = 7; i >= 0; i--)
    {
        cassetteModulationToggle();
        cassetteTimerExpired (BIT_DURATION/2);

        if (byte & (1<<i))
            cassetteModulationToggle();

        cassetteTimerExpired (BIT_DURATION/2);
    }
}

static void outputBlock (uint8_t *data)
{
    int i;

    for (i = 0; i < 8; i++)
        outputByte (0);

    outputByte (0xFF);

    uint8_t sum = 0;

    for (i = 0; i < 64; i++)
    {
        outputByte (data[i]);
        sum += data[i];
    }

    outputByte (sum);
}

static void outputWav(void)
{
    int i;

    /*  Preamble */
    for (i = 0; i < 768; i++)
        outputByte (0);

    recordCount = 1;
    blockCount = (programSize + 1) / 64;

    /*  Output header */
    outputByte (0xFF);
    outputByte (blockCount);
    outputByte (blockCount);

    for (i = 0; i < blockCount; i++)
    {
        outputBlock (program+i*64);
        outputBlock (program+i*64);
    }
}

int main (int argc, char *argv[])
{
    char c;
    char *binFile = NULL;

    while ((c = getopt(argc, argv, "c:e:vbrw")) != -1)
    {
        switch (c)
        {
            case 'c' : options.create = true; binFile = optarg; break;
            case 'e' : options.extract = true; binFile = optarg; break;
            case 'v' : options.verbose = true; break;
            case 'b' : options.basic = true; break;
            case 'r' : options.raw = true; break;
            case 'w' : options.wav = true; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    wavState *wav;

    if (argc - optind < 1)
    {
        printf ("\nTool to read and write wav files for cassette audio\n\n"
                "usage: %s [-c <file>] [-e <file>] [-v] [-b] [-r] [-w] <wav-file>\n"
                "\t where -c=create WAV from <file> (TIFILES or tokenised TI basic)\n"
                "\t       -e=extract to <file>, -v=verbose, -b=decode basic, "
                "-r=raw bits, -w=show wav hdr\n\n", argv[0]);
        return 1;
    }

    if (options.create)
    {
        struct stat statbuf;
        if (stat (argv[optind], &statbuf) != -1)
        {
            fprintf (stderr, "File %s exists, won't over-write\n", argv[optind]);
            return 1;
        }

        programSize = filesReadBinary (binFile, program, MAX_PROGRAM_SIZE,
                                       options.verbose);

        if (programSize < 0)
            return 1;

        cassetteFileOpenWrite (argv[optind]);
        outputWav ();
        cassetteFileCloseWrite ();
    }
    else
    {
        wav = wavFileOpenRead (argv[optind], options.wav);

        preambleSync = true;
        preambleBitsExpected = 3000;
        inputWav (wav);
        wavFileClose (wav);

        if (options.basic)
            decodeBasicProgram (program, blockCount * 64, basic, false);

        if (options.extract)
            filesWriteBinary (binFile, program, blockCount * 64, false);

        if (options.verbose)
            printf ("\n");

        // If basic output selected then don't output status msgs
        if (!options.basic)
            printf ("Found %d blocks in %d record%s%s", blockCount, recordCount,
                    recordCount>1?"s":"",
                    errorsFound?"\n":", no errors were found\n");

        if (errorsFound)
            fprintf (stderr, "*** Errors were found which %s\n", errorsFixable ? "are fixable" : "can't be fixed");
    }

    return 0;
}
