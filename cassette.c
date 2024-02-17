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
 *
 *  Implements cassette audio modulation and WAV file read and write for
 *  cassette operations.  Sound is modulation using a pure frequency modulated
 *  sine wave, which looks and sounds different to the original recordings.
 *  Maybe add a table of values that match the original modulations.  Also
 *  reading just uses zero crossings, no frequency analysis is done.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "sound.h"
#include "interrupt.h"
#include "wav.h"
#include "cassette.h"
#include "trace.h"

/*  Maintain a file sample counter.  For write, this is how many samples we have
 *  generated.  For read, it is how many samples are remaining in the file
 */
static int cassetteSampleCount;

static struct timespec cassetteAudioStart; // The time at which we last read audio

static wavState *cassetteWav;
static int cassetteModulationState;
static int cassetteModulationNext;
static int cassetteModulationReadSamples;

/*
 *  Create audio cassette recording.  Modulates a sine wave based on
 *  the square wave from the cassette output.  Uses a 3-bit shift register to
 *  determine the context of the bit.  A 0 generates the bottom half of a sine
 *  (PI to 2PI), a 1 generates the top half (0 to PI).  If it is different from
 *  previous and next then frequency is doubled.  If it is the same as previous
 *  then it is the second quarter of a sine wave.  If it is the same as next
 *  then it is the first quarter of sine wave.
 */
struct _encoding
{
    int start;
    int duration;
}
encoding[] =
{
    { 0, 0 }, // 000 invalid
    { 3, 1 }, // 001 play freq 270 to 360
    { 0, 2 }, // 010 play freq*2 0 to 180
    { 0, 1 }, // 011 play freq   0 to 90
    { 2, 1 }, // 100 play freq 180 to 270
    { 2, 2 }, // 101 play freq*2 180 to 360
    { 1, 1 }, // 110 play freq 90 to 180
    { 0, 0 }  // 111 invalid
};

void cassetteTimerExpired (int duration)
{
    static double sampleCount = 0;

    if (!cassetteWav)
        return;

    /*  Duration is in nanoseconds */
    sampleCount += 1.0 * CASSETTE_SAMPLE_RATE * duration / 1000000000.0;
    int samples = (int) sampleCount;
    sampleCount -= samples;

    if (wavIsOpenWrite (cassetteWav))
    {
        double angle = (M_PI / 2.0) * encoding[cassetteModulationState].start;
        double change = (M_PI / 2.0) * encoding[cassetteModulationState].duration / samples;

        for (int i = 0; i < samples; i++)
        {
            short sample = CASSETTE_AMPLITUDE * sin (angle + i * change);
            soundAuxData (sample);
            wavWriteSample (cassetteWav, sample);
            cassetteSampleCount ++;
        }

        cassetteModulationState <<= 1;
        cassetteModulationState |= cassetteModulationNext;
        cassetteModulationState &= 0x07;
    }
    else
    {
        /*  Record the number of samples to read that correspond to the time the
         *  timer was running */
        cassetteModulationReadSamples = samples;
    }
}

void cassetteModulationToggle (void)
{
    cassetteModulationNext = 1 - cassetteModulationNext;
}


static uint16_t cassetteModulationRead (void)
{
    static double sampleCount = 0.0;

    /*  If the file is closed (reached EOF) then just return a constantly
     *  flipping bit to get the console out of any infinite loops and realise
     *  there is a data error
     */
    if (!cassetteWav)
    {
        cassetteModulationToggle ();
        return cassetteModulationNext;
    }

    /*  If a timer has expired, it will have set the cassetteModulationReadSamples count
     *  and we know exactly how many samples to read.  If not, then we are in a
     *  busy loop so lookup the elapsed time since we were last called to find
     *  out how many samples to read.  Using elapsed time is not accurate at all
     *  times since our process can be scheduled out etc but for busy loops with
     *  small time increments it should be fine.
     */
    int samples = cassetteModulationReadSamples;
    cassetteModulationReadSamples = 0;

    if (samples)
        clock_gettime (CLOCK_MONOTONIC, &cassetteAudioStart);
    else
    {
        int nsec;
        struct timespec now;

        clock_gettime (CLOCK_MONOTONIC, &now);

        /*  How many nanoseconds have elapsed since we last read the file? */
        nsec = (now.tv_sec - cassetteAudioStart.tv_sec) * 1000000000 + 
                (now.tv_nsec - cassetteAudioStart.tv_nsec);

        double newSamples = 1.0 * nsec * wavSampleRate (cassetteWav) / 1000000000.0; //  - audioReadSamplePos;

        if (newSamples + sampleCount >= 1.0)
        {
            /*  How many samples should we read since we did the last read?  Use
             *  a double to track fractions of samples, then use an int to
             *  create the samples
             */
            sampleCount += newSamples;

            /* Go through samples one-by-one to ensure no zero crossing is
             * missed in case time skips forward.  The audio playback may fall
             * behind and stutter a bit */

            // samples = (int) sampleCount;
            samples = 1;

            sampleCount -= samples;

            /*  Reset the timestamp for the next iteration */
            cassetteAudioStart = now;
        }
    }

    if (samples > 0)
    {
        static int ident;
        static int lastBit;

        for (int i = 0; i < samples; i++)
        {
            short sample = 0;

            sample = wavReadSample (cassetteWav);

            soundAuxData (sample);
            cassetteModulationNext = (sample > 0) ? 1 : 0;
        }

        if (samples>1||lastBit != cassetteModulationNext)
            mprintf (LVL_CASSETTE, "[CS1 smp=%d id=%d]", samples, ident++);
        lastBit = cassetteModulationNext;

        cassetteSampleCount -= samples;

        if (cassetteSampleCount <= 0)
        {
            printf ("CS1 : eof reached\n");
            wavFileClose (cassetteWav);
            cassetteWav = NULL;
        }
    }

    return cassetteModulationNext;
}

void cassetteFileCloseWrite (void)
{
    /*  Write a few zero bits to the audio output to flush out any pending
     *  bits that are to be written.  This is to ensure the audio modulation
     *  state machine has written out all pending bits.  These "dribble"
     *  bits have no effect on the loading operation.
     */

    printf ("closing tape file\n");

    for (int i = 0; i < 8; i++)
    {
        cassetteModulationToggle ();
        cassetteTimerExpired (360000); // 360000 nanosec=1 cycle of 1370Hz @ 44100
        cassetteTimerExpired (360000);
    }

    wavFileClose (cassetteWav);
    cassetteWav = NULL;
}

bool cassetteMotor(int index, uint8_t value)
{
    printf ("cassette motor %d set to %d\n", index-21, value);

    if (value == 0 && cassetteWav)
        cassetteFileCloseWrite();

    return false;
}

bool cassetteAudioGate(int index, uint8_t value)
{
    printf ("cassette audio gate set to %d\n", value);
    return false;
}

void cassetteFileOpenWrite (const char *name)
{
    cassetteWav = wavFileOpenWrite (name, CASSETTE_BITS_PER_SAMPLE);
}

bool cassetteTapeOutput(int index, uint8_t value)
{
    if (!cassetteWav)
        cassetteFileOpenWrite (CASSETTE_FILE_NAME);

    cassetteModulationNext = value;
    return false;
}

uint8_t cassetteTapeInput(int index, uint8_t value)
{
    if (!cassetteWav)
    {
        cassetteWav = wavFileOpenRead (CASSETTE_FILE_NAME, false);

        cassetteSampleCount = wavSampleCount (cassetteWav);
        /*  Initialise the synchronisation time */
        clock_gettime (CLOCK_MONOTONIC, &cassetteAudioStart);
    }

    return cassetteModulationRead ();
}

