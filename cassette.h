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

#ifndef __CASSETTE_H
#define __CASSETTE_H

#include "types.h"

#include "wav.h"

/*  Cassette audio needs to be LOUD.  Removing attenuation */
// #define CASSETTE_AMPLITUDE 16383 // 2^14-1, 3dB attenuation
#define CASSETTE_AMPLITUDE 32767 // 0dB attenuation
#define CASSETTE_SAMPLE_RATE 44100
#define CASSETTE_BITS_PER_SAMPLE    8
#define CASSETTE_FILE_NAME "cassette.wav"

/*  TODO all methods and data in this class need to be completely static until
 *  the CRU callbacks are refactored.  */
class Cassette
{
public:
    static bool motor(int index, uint8_t value);
    static bool audioGate(int index, uint8_t value);
    static bool tapeOutput(int index, uint8_t value);
    static void modulationToggle (void);
    static uint8_t tapeInput(int index, uint8_t value);
    static void timerExpired (int duration);
    static void fileOpenWrite (const char *name);
    static void fileCloseWrite (void);
private:
    /*  Maintain a file sample counter.  For write, this is how many samples we have
     *  generated.  For read, it is how many samples are remaining in the file
     */
    static int _sampleCount;
    static struct timespec _audioStart; // The time at which we last read audio
    static int _modulationState;
    static int _modulationNext;
    static int _modulationReadSamples;
    static WavFile _wavFile;
    static int modulationRead (void);
};

#endif

