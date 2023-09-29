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

#ifndef __SOUND_H
#define __SOUND_H

#include "cpu.h"

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
WAV_FILE_HDR;

void soundInit (void);
// void soundUpdate (void);
int soundRead (int addr, int size);
void soundWrite (int addr, int data, int size);
void soundModulation (int duration);
void soundModulationValue (int value);
void soundModulationToggle (void);
void soundWavFileOpenRead(void);
void soundWavFileOpenWrite(void);
void soundWavFileClose (void);
uint16_t soundModulationRead (void);

#endif

