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

void Cassette::timerExpired (int duration)
{
    static double sampleCount = 0;

    /*  Duration is in nanoseconds */
    sampleCount += 1.0 * CASSETTE_SAMPLE_RATE * duration / 1000000000.0;
    int samples = (int) sampleCount;
    sampleCount -= samples;

    if (_wavFile.isOpenWrite ())
    {
        double angle = (M_PI / 2.0) * encoding[_modulationState].start;
        double change = (M_PI / 2.0) * encoding[_modulationState].duration / samples;

        for (int i = 0; i < samples; i++)
        {
            short sample = CASSETTE_AMPLITUDE * sin (angle + i * change);
            soundAuxData (sample);
            _wavFile.writeSample (sample);
            _sampleCount ++;
        }

        _modulationState <<= 1;
        _modulationState |= _modulationNext;
        _modulationState &= 0x07;
    }
    else
    {
        /*  Record the number of samples to read that correspond to the time the
         *  timer was running */
        _modulationReadSamples = samples;
    }
}

void Cassette::modulationToggle (void)
{
    _modulationNext = 1 - _modulationNext;
}

int Cassette::modulationRead (void)
{
    static double sampleCount = 0.0;

    /*  If the file is closed (reached EOF) then just return a constantly
     *  flipping bit to get the console out of any infinite loops and realise
     *  there is a data error
     */
    if (!_wavFile.isOpen ())
    {
        modulationToggle ();
        return _modulationNext;
    }

    /*  If a timer has expired, it will have set the _modulationReadSamples count
     *  and we know exactly how many samples to read.  If not, then we are in a
     *  busy loop so lookup the elapsed time since we were last called to find
     *  out how many samples to read.  Using elapsed time is not accurate at all
     *  times since our process can be scheduled out etc but for busy loops with
     *  small time increments it should be fine.
     */
    int samples = _modulationReadSamples;
    _modulationReadSamples = 0;

    if (samples)
        clock_gettime (CLOCK_MONOTONIC, &_audioStart);
    else
    {
        int nsec;
        struct timespec now;

        clock_gettime (CLOCK_MONOTONIC, &now);

        /*  How many nanoseconds have elapsed since we last read the file? */
        nsec = (now.tv_sec - _audioStart.tv_sec) * 1000000000 + 
                (now.tv_nsec - _audioStart.tv_nsec);

        double newSamples = 1.0 * nsec * _wavFile.getSampleRate () / 1000000000.0; //  - audioReadSamplePos;

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
            _audioStart = now;
        }
    }

    if (samples > 0)
    {
        static int ident;
        static int lastBit;

        for (int i = 0; i < samples; i++)
        {
            short sample = 0;

            sample = _wavFile.readSample ();

            soundAuxData (sample);
            _modulationNext = (sample > 0) ? 1 : 0;
        }

        if (samples>1||lastBit != _modulationNext)
            mprintf (LVL_CASSETTE, "[CS1 smp=%d id=%d]", samples, ident++);
        lastBit = _modulationNext;

        _sampleCount -= samples;

        if (_sampleCount <= 0)
        {
            printf ("CS1 : eof reached\n");
            _wavFile.close ();
        }
    }

    return _modulationNext;
}

void Cassette::fileCloseWrite (void)
{
    /*  Write a few zero bits to the audio output to flush out any pending
     *  bits that are to be written.  This is to ensure the audio modulation
     *  state machine has written out all pending bits.  These "dribble"
     *  bits have no effect on the loading operation.
     */

    printf ("closing tape file\n");

    for (int i = 0; i < 8; i++)
    {
        modulationToggle ();
        timerExpired (360000); // 360000 nanosec=1 cycle of 1370Hz @ 44100
        timerExpired (360000);
    }

    _wavFile.close ();
}

bool Cassette::motor(int index, uint8_t value)
{
    printf ("cassette motor %d set to %d\n", index-21, value);

    if (value == 0 && _wavFile.isOpen())
        fileCloseWrite();

    return false;
}

bool Cassette::audioGate(int index, uint8_t value)
{
    printf ("cassette audio gate set to %d\n", value);
    return false;
}

void Cassette::fileOpenWrite (const char *name)
{
    // TODO check return
    _wavFile.openWrite (name, CASSETTE_BITS_PER_SAMPLE);
}

bool Cassette::tapeOutput(int index, uint8_t value)
{
    if (!_wavFile.isOpen ())
        fileOpenWrite (CASSETTE_FILE_NAME);

    _modulationNext = value;
    return false;
}

uint8_t Cassette::tapeInput(int index, uint8_t value)
{
    if (!_wavFile.isOpen ())
    {
        _wavFile.openRead (CASSETTE_FILE_NAME, false);

        _sampleCount = _wavFile.getSampleCount ();
        /*  Initialise the synchronisation time */
        clock_gettime (CLOCK_MONOTONIC, &_audioStart);
    }

    return modulationRead ();
}

