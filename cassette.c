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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sound.h"
#include "interrupt.h"
#include "cassette.h"

static bool cassetteFileOpen = false;

bool cassetteMotor(int index, uint8_t value)
{
    printf ("cassette motor %d set to %d\n", index-21, value);

    if (value == 0 && cassetteFileOpen)
    {
        /*  Write a few zero bits to the audio output to flush out any pending
         *  bits that are to be written.  This is to ensure the audio modulation
         *  state machine has written out all pending bits.  These "dribble"
         *  bits have no effect on the loading operation.
         */

        printf ("closing tape file\n");

        for (int i = 0; i < 8; i++)
        {
            soundModulationToggle ();
            soundModulation (360000); // 360000 nanosec=1 cycle of 1370Hz @ 44100
            soundModulation (360000);
        }

        soundWavFileClose ();
        cassetteFileOpen = false;
    }

    return false;
}

bool cassetteAudioGate(int index, uint8_t value)
{
    printf ("cassette audio gate set to %d\n", value);
    return false;
}

bool cassetteTapeOutput(int index, uint8_t value)
{
    if (!cassetteFileOpen)
    {
        soundWavFileOpenWrite ();
        cassetteFileOpen = true;
    }

    soundModulationValue (value);
    return false;
}

uint8_t cassetteTapeInput(int index, uint8_t value)
{
    if (!cassetteFileOpen)
    {
        soundWavFileOpenRead ();
        cassetteFileOpen = true;
    }

    return soundModulationRead ();
}

