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
 *  Manages WAV files.  Implements functions to open/close/read/write.
 *  Different rates, bits per samples and channels are supported for reading
 *  files but writing files are fixed at 44100, 16 bits and 1 channel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "trace.h"
#include "wav.h"

#define WAV_DEFAULT_CHANNELS    1
#define WAV_DEFAULT_RATE        44100

WavFile::WavFile ()
{
    _fp = nullptr;
    _channels = 1;
    _bits = 16;
    _rate = WAV_DEFAULT_RATE;
    _sampleCount = 0;
    _write = false;
    _blockSize = 2;
}

void WavFile::dumpHeader (wavHeader hdr)
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

bool WavFile::openRead (const char *name, bool showParams)
{
    wavHeader hdr;

    if ((_fp = fopen (name, "r")) == NULL)
    {
        printf ("Failed to open file %s for read\n", name);
        return false;
    }

    // printf ("opened file %s for read\n", name);
    fread (&hdr, sizeof hdr, 1, _fp);

    if (showParams)
        dumpHeader (hdr);

    if ((hdr.numChannels != 1 && hdr.numChannels != 2) ||
        (hdr.bitsSample != 8 && hdr.bitsSample != 16))
    {
        printf ("Unsupported audio file format\n");
        printf ("channels=%d,bits=%d\n", hdr.numChannels, hdr.bitsSample);
        fclose (_fp);
        _fp = nullptr;
        return false;
    }

    _channels = hdr.numChannels;
    _rate = hdr.sampleRate;
    _bits = hdr.bitsSample;
    _rate = hdr.sampleRate;
    _blockSize = _channels * (_bits / 8);
    _sampleCount = hdr.dataSize / _blockSize;
    return true;
}

bool WavFile::openWrite (const char *name, int bits)
{
    wavHeader hdr;

    if ((_fp = fopen (name, "w")) == NULL)
    {
        printf ("Failed to open file %s for write\n", name);
        return false;
    }

    printf ("opened %s for write\n", name);

    _channels = WAV_DEFAULT_CHANNELS;
    _rate = WAV_DEFAULT_RATE;
    _bits = bits;
    _write = true;
    _blockSize = _channels * (_bits / 8);

    /*  Write dummy header for now, populate later */
    fwrite (&hdr, sizeof hdr, 1, _fp);
    return true;
}

void WavFile::close ()
{
    wavHeader hdr;

    // printf ("closing file\n");

    if (_write)
    {
        fseek (_fp, SEEK_SET, 0);

        memcpy (hdr.riff, "RIFF", 4);
        hdr.fileSize = _sampleCount * _blockSize + 44 - 8;
        memcpy (hdr.wave, "WAVE", 4);
        memcpy (hdr.fmt, "fmt ", 4);
        hdr.waveSize = 16;
        hdr.waveType = 1;
        hdr.numChannels = _channels;
        hdr.sampleRate = _rate;
        hdr.bytesSec = _rate * _blockSize;
        hdr.blockAlign = _blockSize;
        hdr.bitsSample = _bits;
        memcpy (hdr.data, "data", 4);
        hdr.dataSize = _sampleCount * _blockSize;

    // if (showParams)
        dumpHeader (hdr);

        fwrite (&hdr, sizeof hdr, 1, _fp);
    }

    fclose (_fp);
    _fp = nullptr;
}

int16_t WavFile::readSample ()
{
    int16_t sample;

    fread (_block, _blockSize, 1, _fp);

    /*  We support reading 2 channel audio but we only look at the first channel
     */
    if (_bits == 8)
    {
        /*  Supporting reading files that are encoded in 8-bit by shifting
         *  the data left 8 bits.  Presumes little endian architecture.
         */
        sample = (int8_t) _block[0];
        sample = (sample+128)<< 8;
    }
    else
        sample = *(int16_t*)_block;

    return sample;
}

void WavFile::writeSample (int16_t sample)
{
    /*  Assume writes are single channel */
    if (_bits == 8)
        *(int8_t*)_block = (sample>>8)-128;
    else
        *(int16_t*)_block = sample;

    fwrite (_block, _blockSize, 1, _fp);
    _sampleCount++;
}

