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
 *  Emulate audio from the TMS9919 chip by generating samples and playing them
 *  through pulse audio.  Audio tones 1 thru 3 are straightforward but periodic
 *  and white noise are not as easy.  This guide has some useful info, but noise
 *  implementation is sitll not quite there : https://www.smspower.org/Development/SN76489
 *
 *  Also implements cassette audio modulation and WAV file read and write for
 *  cassette operations.  Sound is modulation using a pure frequency modualated
 *  sine wave, which looks and sounds different to the original recordings.
 *  Maybe add a table of values that match the original modulations.  Also
 *  reading just uses zero crossings, no frequency analysis is done.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pulse/simple.h>

#include "sound.h"
#include "trace.h"
#include "status.h"

/*  The TMS9919 / SN76489 is designed to be clocked at this frequency.  We need
 *  this value to translate into audio frequencies.
 */
#define CLOCK_FREQUENCY 111861

/*  The frequency at which we are playing samples to pulse audio
 */
#define AUDIO_FREQUENCY 44100

/* At 44,100Hz we need to generate 882 samples per call as we are called every
 * 20msec.
 */
#define SAMPLE_COUNT 882 // 44,100 divided by 50

/* We seem to be getting underruns.  Upping to 1000 to debug.
 */
// #define SAMPLE_COUNT 1000 // 44,100 divided by 50

/* We are summing up to 4 sound sources to generate a value from -32768 to +32767
 * so we assume a maximum volume (minimum attenuation) of any one source to have
 * a multplier of 8191 and we divide that by 15 (0x0f) to get 546.
 */
#define SAMPLE_AMPLITUDE 546 // 8191 div 15

#define AUX_SAMPLE_FIFO (SAMPLE_COUNT*50)

#define AUX_AMPLITUDE 8191

#define TAPE_DEFAULT_BITS_PER_SAMPLE    16

static int cassetteBitsPerSample = TAPE_DEFAULT_BITS_PER_SAMPLE;

typedef struct
{
    int requestedPeriod;
    int period;
    int shift;
    double angle;
    int counter;
    int requestedAmplitude;
    int amplitude;
    int whiteNoise;
    int useTone3Freq;
}
toneInfo;

static FILE *fpCassette;
static bool wavFileWrite = false;

/*  Maintain a file sample counter.  For write, this is how many samples we have
 *  generated.  For read, it is how many samples are remaining in the file
 */
static int cassetteSampleCount;

static struct timespec startAudioRead; // The time at which we last read audio

void soundWavFileOpenRead (void)
{
    WAV_FILE_HDR hdr;

    printf ("opening file for read\n");

    fpCassette = fopen ("cassette.wav", "r");
    fread (&hdr, sizeof hdr, 1, fpCassette);

    if (hdr.numChannels != 1 || hdr.sampleRate != 44100)
    {
        printf ("Unsupported audio file format\n");
        printf ("channels=%d,rate=%d\n", hdr.numChannels, hdr.sampleRate);
        fclose (fpCassette);
        fpCassette = NULL;
        return;
    }

    cassetteBitsPerSample = hdr.bitsSample;

    cassetteSampleCount = hdr.dataSize / 2;

    /*  Initialise the synchronisation time */
    clock_gettime (CLOCK_MONOTONIC, &startAudioRead);
}

void soundWavFileOpenWrite (void)
{
    WAV_FILE_HDR hdr;

    printf ("opening file for write\n");

    cassetteBitsPerSample = TAPE_DEFAULT_BITS_PER_SAMPLE;
    fpCassette = fopen ("cassette.wav", "w");
    /*  Write dummy header for now, populate later */
    fwrite (&hdr, sizeof hdr, 1, fpCassette);
    wavFileWrite = true;
}

void soundWavFileClose (void)
{
    WAV_FILE_HDR hdr;

    printf ("closing file\n");

    if (wavFileWrite)
    {
        fseek (fpCassette, SEEK_SET, 0);

        memcpy (hdr.riff, "RIFF", 4);
        hdr.fileSize = cassetteSampleCount * 2 + 44 - 8;
        memcpy (hdr.wave, "WAVE", 4);
        memcpy (hdr.fmt, "fmt ", 4);
        hdr.waveSize = 16;
        hdr.waveType = 1;
        hdr.numChannels = 1;
        hdr.sampleRate = AUDIO_FREQUENCY;
        hdr.bytesSec = AUDIO_FREQUENCY * 2;
        hdr.blockAlign = 2;
        hdr.bitsSample = 16;
        memcpy (hdr.data, "data", 4);
        hdr.dataSize = cassetteSampleCount * 2;

        fwrite (&hdr, sizeof hdr, 1, fpCassette);
    }

    fclose (fpCassette);
    fpCassette = NULL;
}

static short cassetteAudio[AUX_SAMPLE_FIFO];
static int cassetteAudioHead;
static int cassetteAudioTail;

static toneInfo tones[4];

/*  XNOR truth table */
int xnor[4] = { 1, 0, 0, 1 };

static pa_simple *pulseAudioHandle;
static pa_sample_spec pulseAudioSpec;

static short generateTone (toneInfo *tone, bool noise)
{
    short sample;
    int bit;

    /*  To avoid pops in the audio, we only make changes to amplitude at the
     *  beginning or end of a cycle.  If a change in amplitude or frequency have
     *  been requested, do this only when counter is zero so we don't do it in
     *  the middle of a cycle
     */
    if (tone->counter == 0)
    {
        tone->amplitude = tone->requestedAmplitude;
        tone->period = tone->requestedPeriod;
    }

    if (tone->amplitude == 0 || tone->period == 0)
        return 0;

    double angle = (2 * M_PI * tone->counter) / tone->period;

    if (noise)
    {
        tone->counter++;

        bit = tone->shift & 1;
        sample = (bit * 2 - 1) * tone->amplitude;

        /*  Pulse audio doesn't seem to like if we send repeated samples so
         *  add a teeny bit of sine wave on top of square wave to make it
         *  vary
         */
        sample += tone->amplitude / 20 * sin (angle);

        if (tone->counter >= tone->period)
        {
            tone->shift >>= 1;

            if (tone->whiteNoise)
            {
                /*  I am not 100% sure this is the bit pattern used by the
                 *  TMS9919 but the sound seems reasonable
                 */
                tone->shift |= xnor[tone->shift & 0x03] << 15;
            }
            else
            {
                tone->shift |= bit << 15;
            }

            tone->counter %= tone->period;
        }
    }
    else
    {
        tone->counter++;
        tone->counter %= tone->period;
        sample = tone->amplitude * sin (angle);
    }

    return sample;
}

static bool updateActiveToneGenerators (toneInfo *tone)
{
    if (tone->requestedAmplitude == 0 && tone->amplitude == 0)
        return false;

    return true;
}

static short generateSample (void)
{
    int i;
    short sample = 0;

    /*  Generate data for each tone generator, index 3 is the noise generator */
    for (i = 0; i < 4; i++)
        sample += generateTone (&tones[i], i == 3);

    return sample;
}

/*  Every 20 msec, generate data to feed pulse audio device using a combination
 *  from currently active tone and noise generators.
 */
void soundUpdate (void)
{
    int i;
    bool anyActive = false;
    bool cassette = false;

    if (pulseAudioHandle == NULL)
        return;

    for (i = 0; i < 4; i++)
    {
        anyActive = updateActiveToneGenerators (&tones[i]) || anyActive;
        
        /* Max amplitude of any channel is 8191 so we divide by 82 to give a
         * rough percentage for readibility 
         */
        statusSoundUpdate (i, tones[i].amplitude / 82, tones[i].period);
    }

    int cassetteSampleCount = (AUX_SAMPLE_FIFO + cassetteAudioHead - cassetteAudioTail) % AUX_SAMPLE_FIFO;

    if (cassetteSampleCount >= SAMPLE_COUNT)
        cassette = true;

    if (!anyActive && !cassette)
        return;

    /*  Create an array of samples to play to pulse audio.    The values are
     *  signed 16-bit so for one channels we have 2 bytes for sample.
     */
    int16_t sampleData[SAMPLE_COUNT];
    for (i = 0; i < SAMPLE_COUNT; i++)
    {
        int16_t sample;

        if (cassette)
        {
            sample = cassetteAudio[cassetteAudioTail];
            cassetteAudioTail++;
            cassetteAudioTail %= AUX_SAMPLE_FIFO;
        }
        else
            sample = generateSample ();

        sampleData[i] = sample;
    }

    pa_simple_write (pulseAudioHandle, sampleData, 2*SAMPLE_COUNT, NULL);
}

void soundInit (void)
{
    pulseAudioSpec.format = PA_SAMPLE_S16NE;
    pulseAudioSpec.channels = 1;
    pulseAudioSpec.rate = AUDIO_FREQUENCY;

    pulseAudioHandle = pa_simple_new(NULL,               // Use the default server.
                      "TI99",           // Our application's name.
                      PA_STREAM_PLAYBACK,
                      NULL,               // Use the default device.
                      "Games",            // Description of our stream.
                      &pulseAudioSpec,                // Our sample format.
                      NULL,               // Use default channel map
                      NULL,               // Use default buffering attributes.
                      NULL               // Ignore error code.
                      );
}

int soundRead (int addr, int size)
{
    return 0;
}

void soundWrite (int addr, int data, int size)
{
    static int latchedData = -1;
    int channel;

    if ((data & 0x90) == 0x90)
    {
        channel = (data & 0x60) >> 5;
        tones[channel].requestedAmplitude = SAMPLE_AMPLITUDE * (0x0f - (data & 0x0f));
        mprintf (LVL_SOUND, "tone 0 requestedAmplitude set to %d\n", tones[channel].requestedAmplitude);
        return;
    }

    /*  Frequency change for tone 1, 2 and 3 are 2-byte commands so latch the
     *  byte and return
     */
    if (latchedData == -1 && (data & 0x80) == 0x80 && (data & 0xE0) != 0xE0)
    {
        latchedData = data;
        return;
    }

    if (latchedData == -1)
        mprintf (LVL_SOUND, "SOUND data=%02X", data);
    else
        mprintf (LVL_SOUND, "SOUND data=%02X,%02X", latchedData, data);

    if ((data & 0xF0) == 0xE0)
    {
        tones[3].shift = 0x8000;
        tones[3].whiteNoise = (data & 0x04) != 0;

        if ((data & 0x03) == 0x03)
            tones[3].useTone3Freq = 1;
        else
        {
            tones[3].useTone3Freq = 0;
            tones[3].period = AUDIO_FREQUENCY / (1748 * (3 - (data & 0x03)));
        }

        mprintf (LVL_SOUND, " noise period=%d\n", tones[3].period);
        return;
    }

    channel = (latchedData & 0x60) >> 5;

    toneInfo *tone = &tones[channel];
    data = (data << 4) | (latchedData & 0x0f);
    latchedData = -1;
    mprintf(LVL_SOUND, " freqdata=%d\n", data);
    tone->requestedPeriod = data * AUDIO_FREQUENCY / CLOCK_FREQUENCY;
    mprintf (LVL_SOUND, "tone %d set to [freq %%d], period %d\n", channel,
    tone->requestedPeriod);

    if (channel == 2 && tones[3].useTone3Freq)
    {
        tones[3].requestedPeriod = tone->requestedPeriod;
    }
}

/*
 *  Create audio cassette (cassette basically).  Modulates a sine wave based on
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

static int cassetteModulationState;
static int cassetteModulationNext;
static int cassetteModulationReadSamples;

void soundModulation (int duration)
{
    static double sampleCount = 0;

    if (!fpCassette)
        return;

    sampleCount += 1.0 * AUDIO_FREQUENCY * duration / 1000000000.0;
    int samples = (int) sampleCount;
    sampleCount -= samples;

    if (wavFileWrite)
    {
        double angle = (M_PI / 2.0) * encoding[cassetteModulationState].start;
        double change = (M_PI / 2.0) * encoding[cassetteModulationState].duration / samples;

        for (int i = 0; i < samples; i++)
        {
            short sample = 16383 * sin (angle + i * change);
            cassetteAudio[cassetteAudioHead++] = sample;
            cassetteAudioHead %= AUX_SAMPLE_FIFO;

            fwrite (&sample, 2, 1, fpCassette);
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

void soundModulationValue (int value)
{
    cassetteModulationNext = value;
}

void soundModulationToggle (void)
{
    cassetteModulationNext = 1 - cassetteModulationNext;
}

uint16_t soundModulationRead (void)
{
    static double sampleCount = 0.0;

    /*  If the file is closed (reached EOF) then just return a constantly
     *  flipping bit to get the console out of any infinite loops and realise
     *  there is a data error
     */
    if (!fpCassette)
    {
        soundModulationToggle();
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
        clock_gettime (CLOCK_MONOTONIC, &startAudioRead);
    else
    {
        int nsec;
        struct timespec now;

        clock_gettime (CLOCK_MONOTONIC, &now);

        /*  How many nanoseconds have elapsed since we last read the file? */
        nsec = (now.tv_sec - startAudioRead.tv_sec) * 1000000000 + 
                (now.tv_nsec - startAudioRead.tv_nsec);

        double samplesPending = 1.0 * nsec * AUDIO_FREQUENCY / 1000000000.0; //  - audioReadSamplePos;

        if (samplesPending >= 1.0)
        {
            /*  How many samples should we read since we did the last read?  Use
             *  a double to track fractions of samples, then use an int to
             *  create the samples
             */
            sampleCount += samplesPending;
            samples = (int) sampleCount;
            sampleCount -= samples;

            /*  Reset the timestamp for the next iteration */
            startAudioRead = now;
        }
    }

    if (samples > 0)
    {
        for (int i = 0; i < samples; i++)
        {
            short sample = 0;

            /*  Supporting reading files that are encoded in 8-bit by shifting
             *  the data left 8 bits.  Presumes little endian architecture.
             */
            if (cassetteBitsPerSample == 8)
            {
                fread (&sample, 1, 1, fpCassette);
                sample <<= 8;
            }
            else
                fread (&sample, 2, 1, fpCassette);

            cassetteAudio[cassetteAudioHead] = sample;
            cassetteModulationNext = (sample > 0) ? 1 : 0;
            cassetteAudioHead++;
            cassetteAudioHead %= AUX_SAMPLE_FIFO;
        }

        cassetteSampleCount -= samples;

        if (cassetteSampleCount <= 0)
        {
            printf ("CS1 : eof reached\n");
            fclose (fpCassette);
            fpCassette = NULL;
        }
    }

    return cassetteModulationNext;
}

