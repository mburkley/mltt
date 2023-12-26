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
 *  Manages WAV files.  Implements functions to open/close/read/write.
 *  Different rates, bits per samples and channels are supported for reading
 *  files but writing files are fixed at 44100, 16 bits and 1 channel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "trace.h"

#define WAV_DEFAULT_CHANNELS    1
#define WAV_DEFAULT_RATE        44100

typedef struct
{
    char riff[4];
    unsigned int fileSize;
    char wave[4];
    char fmt[4];
    unsigned int waveSize;
    short waveType;
    short numChannels;
    unsigned int sampleRate;
    unsigned int bytesSec;
    short blockAlign;
    short bitsSample;
    char data[4];
    unsigned int dataSize;
}
wavHeader;

typedef struct _wavState
{
    FILE *fp;
    int channels;
    int bits;
    int rate;
    int sampleCount;
    bool write;
    int blockSize;
    uint8_t block[4]; // Maximum sample block size is 16 bits, 2 channels
}
wavState;

static void dumpHeader (wavHeader hdr)
{
    printf ("RIFF=%-4.4s\n", hdr.riff);
    printf ("  file size=%d\n\n", hdr.fileSize);
    printf ("WAVE=%-4.4s\n", hdr.wave);
    printf ("  FMT=%-4.4s\n", hdr.fmt);
    printf ("  wave size=%d\n", hdr.waveSize);
    printf ("  wave type=%d\n", hdr.waveType);
    printf ("  channels=%d\n", hdr.numChannels);
    printf ("  sample rate=%d\n", hdr.sampleRate);
    printf ("  bytes per sec=%d\n", hdr.bytesSec);
    printf ("  block align=%d\n", hdr.blockAlign);
    printf ("  bits per sample=%d\n\n", hdr.bitsSample);
    printf ("DATA=%-4.4s\n", hdr.data);
    printf ("  data size=%d\n\n", hdr.dataSize);
}

wavState *wavFileOpenRead (const char *name, bool showParams)
{
    wavHeader hdr;
    wavState *state;

    if ((state = calloc (1, sizeof (wavState))) == NULL)
    {
        halt ("Failed to allocate wav state\n");
        return NULL;
    }

    if ((state->fp = fopen (name, "r")) == NULL)
    {
        printf ("Failed to open file %s for read\n", name);
        free (state);
        return NULL;
    }

    printf ("opened file %s for read\n", name);
    fread (&hdr, sizeof hdr, 1, state->fp);

    if (showParams)
        dumpHeader (hdr);

    if ((hdr.numChannels != 1 && hdr.numChannels != 2) ||
        (hdr.bitsSample != 8 && hdr.bitsSample != 16))
    {
        printf ("Unsupported audio file format\n");
        printf ("channels=%d,bits=%d\n", hdr.numChannels, hdr.bitsSample);
        fclose (state->fp);
        state->fp = NULL;
        free (state);
        return NULL;
    }

    state->channels = hdr.numChannels;
    state->rate = hdr.sampleRate;
    state->bits = hdr.bitsSample;
    state->rate = hdr.sampleRate;
    state->blockSize = state->channels * (state->bits / 8);
    state->sampleCount = hdr.dataSize / state->blockSize;

    return state;
}

wavState *wavFileOpenWrite (const char *name, int bits)
{
    wavHeader hdr;
    wavState *state;

    if ((state = calloc (1, sizeof (wavState))) == NULL)
    {
        halt ("Failed to allocate wav state\n");
        return NULL;
    }

    if ((state->fp = fopen (name, "w")) == NULL)
    {
        printf ("Failed to open file %s for write\n", name);
        return NULL;
    }

    printf ("opened %s for write\n", name);

    state->channels = WAV_DEFAULT_CHANNELS;
    state->rate = WAV_DEFAULT_RATE;
    state->bits = bits;
    state->write = true;
    state->blockSize = state->channels * (state->bits / 8);

    /*  Write dummy header for now, populate later */
    fwrite (&hdr, sizeof hdr, 1, state->fp);
    return state;
}

void wavFileClose (wavState *state)
{
    wavHeader hdr;

    printf ("closing file\n");

    if (state->write)
    {
        fseek (state->fp, SEEK_SET, 0);

        memcpy (hdr.riff, "RIFF", 4);
        hdr.fileSize = state->sampleCount * state->blockSize + 44 - 8;
        memcpy (hdr.wave, "WAVE", 4);
        memcpy (hdr.fmt, "fmt ", 4);
        hdr.waveSize = 16;
        hdr.waveType = 1;
        hdr.numChannels = state->channels;
        hdr.sampleRate = state->rate;
        hdr.bytesSec = state->rate * state->blockSize;
        hdr.blockAlign = state->blockSize;
        hdr.bitsSample = state->bits;
        memcpy (hdr.data, "data", 4);
        hdr.dataSize = state->sampleCount * state->blockSize;

    // if (showParams)
        dumpHeader (hdr);

        fwrite (&hdr, sizeof hdr, 1, state->fp);
    }

    fclose (state->fp);
    free (state);
}

int wavSampleCount (wavState *state)
{
    return state->sampleCount;
}

bool wavIsOpenWrite (wavState *state)
{
    return state->write;
}

int wavSampleRate (wavState *state)
{
    return state->rate;
}

int16_t wavReadSample (wavState *state)
{
    int16_t sample;

    fread (state->block, state->blockSize, 1, state->fp);

    /*  We support reading 2 channel audio but we only look at the first channel
     */
    if (state->bits == 8)
    {
        /*  Supporting reading files that are encoded in 8-bit by shifting
         *  the data left 8 bits.  Presumes little endian architecture.
         */
        sample = (int8_t) state->block[0];
        sample = (sample+128)<< 8;
    }
    else
        sample = *(int16_t*)state->block;

    return sample;
}

void wavWriteSample (wavState *state, int16_t sample)
{
    /*  Assume writes are single channel */
    if (state->bits == 8)
        *(int8_t*)state->block = (sample>>8)-128;
    else
        *(int16_t*)state->block = sample;

    fwrite (state->block, state->blockSize, 1, state->fp);
    state->sampleCount++;
}

