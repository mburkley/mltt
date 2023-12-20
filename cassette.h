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

#ifndef __CASSETTE_H
#define __CASSETTE_H

#include "types.h"

#define CASSETTE_AMPLITUDE 16383 // 2^14-1, 3dB attenuation
#define CASSETTE_SAMPLE_RATE 44100
#define CASSETTE_BITS_PER_SAMPLE    8
#define CASSETTE_FILE_NAME "cassette.wav"

bool cassetteMotor(int index, uint8_t value);
bool cassetteAudioGate(int index, uint8_t value);
bool cassetteTapeOutput(int index, uint8_t value);
void cassetteModulationToggle (void);
uint8_t cassetteTapeInput(int index, uint8_t value);
void cassetteModulation (int duration);
void cassetteFileOpenWrite (const char *name);
void cassetteFileCloseWrite (void);

#endif

