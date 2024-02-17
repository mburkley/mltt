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
 *  Emulate audio from the TMS9919 chip by generating samples and playing them
 *  through pulse audio.  Audio tones 1 thru 3 are straightforward but periodic
 *  and white noise are not as easy.  This guide has some useful info :
 *  https://www.smspower.org/Development/SN76489
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <pulse/simple.h>

#include "types.h"
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
#define SAMPLE_COUNT 441 // 44,100 divided by 100 (10 msec)

/* We are summing up to 4 sound sources to generate a value from -32768 to +32767
 * so we assume a maximum volume (minimum attenuation) of any one source to have
 * a multiplier of 8191 (-6dB) and we divide that by 15 (0x0f) to get 546.
 */
#define SAMPLE_AMPLITUDE 546 // 8191 div 15

/*  We allow auxilliary audio inputs which we will mix with the sound generators
 *  to be played.  This is typically the cassette sound but could be anything.
 *  Only one source is supported though.
 */
#define AUX_SAMPLE_FIFO (SAMPLE_COUNT*50)
#define AUX_AMPLITUDE 8191

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

static short soundAux[AUX_SAMPLE_FIFO];
static int soundAuxHead;
static int soundAuxTail;

static toneInfo tones[4];

/*  XNOR truth table */
int xnor[4] = { 1, 0, 0, 1 };

static pthread_mutex_t soundAuxMutex = PTHREAD_MUTEX_INITIALIZER;

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

/*  Every 10 msec, generate data to feed pulse audio device using a combination
 *  from currently active tone and noise generators.
 */
static bool soundUpdate (pa_simple *pulseAudioHandle)
{
    int i;
    bool anyActive = false;
    bool auxAvailable = false;

    for (i = 0; i < 4; i++)
    {
        anyActive = updateActiveToneGenerators (&tones[i]) || anyActive;
        
        /* Max amplitude of any channel is 8191 so we divide by 82 to give a
         * rough percentage for readibility 
         */
        statusSoundUpdate (i, tones[i].amplitude / 82, tones[i].period);
    }

    pthread_mutex_lock (&soundAuxMutex);
    int auxSampleCount = (AUX_SAMPLE_FIFO + soundAuxHead - soundAuxTail) % AUX_SAMPLE_FIFO;
    pthread_mutex_unlock (&soundAuxMutex);

    if (auxSampleCount >= SAMPLE_COUNT)
        auxAvailable = true;

    if (!anyActive && !auxAvailable)
    {
        return false;
    }

    /*  Create an array of samples to play to pulse audio.    The values are
     *  signed 16-bit so for one channels we have 2 bytes for sample.
     */
    int16_t sampleData[SAMPLE_COUNT];
    pthread_mutex_lock (&soundAuxMutex);
    for (i = 0; i < SAMPLE_COUNT; i++)
    {
        int16_t sample;

        if (auxAvailable)
        {
            sample = soundAux[soundAuxTail];
            soundAuxTail++;
            soundAuxTail %= AUX_SAMPLE_FIFO;
        }
        else
            sample = generateSample ();

        sampleData[i] = sample;
    }

    pthread_mutex_unlock (&soundAuxMutex);
    pa_simple_write (pulseAudioHandle, sampleData, 2*SAMPLE_COUNT, NULL);

    return true;
}

static bool soundThreadRunning = false;
static pthread_t audioThread;

static void *soundThread (void *arg)
{
    pa_simple *pulseAudioHandle = (pa_simple*) arg;

    while (soundThreadRunning)
    {
        if (!soundUpdate (pulseAudioHandle))
            usleep (10000);
    }

    return NULL;
}

/*  Add a sample to the auxilliary sample queue.  Since this may come from a
 *  different thread we apply a lock before manipulating head and tail pointers
 */
void soundAuxData (int16_t sample)
{
    pthread_mutex_lock (&soundAuxMutex);
    soundAux[soundAuxHead++] = sample;
    soundAuxHead %= AUX_SAMPLE_FIFO;
    pthread_mutex_unlock (&soundAuxMutex);
}

void soundInit (void)
{
    pa_simple *pulseAudioHandle;
    static pa_sample_spec pulseAudioSpec;

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

    if (pulseAudioHandle == NULL)
        halt ("pulse audio handle");

    soundThreadRunning = true;

    if (pthread_create (&audioThread, NULL, soundThread, pulseAudioHandle) != 0)
        halt ("create sound thread");
}

void soundClose (void)
{
    soundThreadRunning = false;
    pthread_join (audioThread, NULL);
}

uint16_t soundRead (uint8_t *ptr, uint16_t addr, int size)
{
    return 0;
}

void soundWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size)
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

