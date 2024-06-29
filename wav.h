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

#ifndef __WAV_H
#define __WAV_H

#include "types.h"

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


class WavFile
{
public:
    WavFile ();
    bool openRead (const char *name, bool showParams);
    bool openWrite (const char *name, int bits);
    void close ();
    bool isOpen () { return _fp != nullptr; }
    bool isOpenWrite () { return _write; }
    int getSampleCount () { return _sampleCount; }
    int getSampleRate () { return _rate; }
    int getSamplePosition () { return _samplePosition; }
    int16_t readSample ();
    void writeSample (int16_t sample);

private:
    FILE *_fp;
    int _channels;
    int _bits;
    int _rate;
    int _sampleCount;
    int _samplePosition;
    bool _write;
    int _blockSize;
    uint8_t _block[4]; // Maximum sample block size is 16 bits, 2 channels
    void dumpHeader (wavHeader hdr);
};

#endif

