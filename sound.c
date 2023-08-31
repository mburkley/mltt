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
 */

#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pulse/simple.h>

#include "sound.h"
#include "trace.h"

/*  The TMS9919 / SN76489 is designed to be clocked at this frequency.  We need
 *  this value to translate into audio frequencies.
 */
#define CLOCK_FREQUENCY 111861

/*  The frequency at which we are playing samples to pulse audio
 */
#define AUDIO_FREQUENCY 44100

/* At 44,100Hz we need to generate 882 samples per call as we are called every
 * 50msec.
 */
#define SAMPLE_COUNT 882 // 44,100 divided by 50

/* We are summing up to 4 sound sources to generate a value from -32768 to +32767
 * so we assume a maximum volume (minimum attenuation) of any one source to have
 * a multplier of 8191 and we divide that by 15 (0x0f) to get 546.
 */
#define SAMPLE_AMPLITUDE 546 // 8191 div 15

/* Set a threshold for "quietness" where it is safe to change amplitudes to
 * avoid "pops"
 */
#define QUIET_THRESHOLD 100

typedef struct
{
    int period;
    int shift;
    double angle;
    int counter;
    int requestedAmplitude;
    int currentAmplitude;
    int whiteNoise;
    int useTone3Freq;
}
toneInfo;

short noiseData[AUDIO_FREQUENCY];

toneInfo tones[4];

/*  XNOR truth table */
int xnor[4] = { 1, 0, 0, 1 };

static pa_simple *pulseAudioHandle;
static pa_sample_spec pulseAudioSpec;

/*  Play a sample to pulse audio.  We are in stereo (2-channel) so play the same
 *  data to each channel.
 */
static void sampleToPulseAudio (unsigned short data)
{
    short sample[2];
    sample[0] = data;
    sample[1] = data;
    pa_simple_write (pulseAudioHandle, &sample, 4, NULL);
}

static short generateTone (toneInfo *tone, bool noise)
{
    double pi = 3.14159;
    short sample;
    int bit;

    if (tone->currentAmplitude == 0 || tone->period == 0)
        return 0;

    if (noise)
    {
        tone->counter++;

        if (tone->counter >= tone->period)
        {
            bit = tone->shift & 1;
            sample = (bit * 2 - 1) * tone->currentAmplitude;
            tone->shift >>= 1;

            if (tone->whiteNoise)
            {
                tone->shift |= xnor[tone->shift & 0x03] << 15;
            }
            else
            {
                tone->shift |= bit << 15;
            }

            tone->counter %= tone->period;
            tone->shift = 0x8000;
        }
    }
    else
    {
        tone->counter++;
        tone->counter %= tone->period;
        tone->angle = (2 * pi * tone->counter) / tone->period;
        // sine wave
        sample = tone->currentAmplitude * sin (tone->angle);
    }

    /*  If a change in amplitude has been requested, do this during a quiet time
     *  in the output (unless its noise in which case it doesn't matter)
     */
    if (tone->requestedAmplitude != tone->currentAmplitude &&
        (noise || (sample > -QUIET_THRESHOLD && sample < QUIET_THRESHOLD)))
    {
        tone->currentAmplitude = tone->requestedAmplitude;
    }

    return sample;
}

/*  To avoid pops in the audio, we only make changes to amplitude at the
 *  beginning or end of a cycle.  If going from zero (inactive) amplitude we set
 *  the angle to zero and only change amplitude if at low energy part of the
 *  current cycle
 */
static bool updateActiveToneGenerators (toneInfo *tone)
{
    if (tone->requestedAmplitude == 0 && tone->currentAmplitude == 0)
        return false;

    if (tone->requestedAmplitude > 0 && tone->currentAmplitude == 0)
    {
        tone->currentAmplitude = tone->requestedAmplitude;
        tone->angle = 0.0;
    }

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

    if (pulseAudioHandle == NULL)
        return;

    for (i = 0; i < 4; i++)
        anyActive = updateActiveToneGenerators (&tones[i]) || anyActive;

    if (!anyActive)
        return;

    for (i = 0; i < SAMPLE_COUNT; i++)
    {
        short sample = generateSample ();
        sampleToPulseAudio (sample);
    }
}

void soundInit (void)
{
    int i;

    for (i = 0; i < AUDIO_FREQUENCY; i++)
        noiseData[i] = (rand() % (SAMPLE_AMPLITUDE * 2)) - SAMPLE_AMPLITUDE;

    pulseAudioSpec.format = PA_SAMPLE_S16NE;
    pulseAudioSpec.channels = 2;
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
        printf ("SOUND data=%02X", data);
    else
        printf ("SOUND data=%02X,%02X", latchedData, data);

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

        printf (" noise period=%d\n", tones[3].period);
        return;
    }

    channel = (latchedData & 0x60) >> 5;

    toneInfo *tone = &tones[channel];
    data = (data << 4) | (latchedData & 0x0f);
    latchedData = -1;
    printf(" freqdata=%d\n", data);
    tone->period = data * AUDIO_FREQUENCY / CLOCK_FREQUENCY;
    mprintf (LVL_SOUND, "tone %d set to [freq %%d], period %d\n", channel,
    tone->period);

    if (channel == 2 && tones[3].useTone3Freq)
    {
        tones[3].period = tone->period;
    }
}


