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
 *  Implements TMS9901 interrupt generation.  Even though the TI994/A is
 *  hardwired to generate only one interrupt level, we implement 16 for
 *  completeness.  Also implements TMS9901 timer operations.
 *
 *  TMS9901 data sheet:
 *    http://www.stuartconner.me.uk/tms99110_breadboard/downloads/tms9901_datasheet.pdf
 */

#include <stdio.h>
#include <stdbool.h>

#include "trace.h"
#include "cru.h"
#include "interrupt.h"
#include "timer.h"
#include "sound.h"

static struct
{
    int intActive[16];
    int intDisabled[16];
    bool timerMode;
    int timer;
    int timerSnapshot;
}
tms9901;

/*  Return the highest priority interrupt pending */
int interruptLevel (int mask)
{
    int i;

    for (i = 0; i <= mask; i++)
    {
        if (tms9901.intActive[i] ) // && !tms9901.intDisabled[i])
        {
            mprintf (LVL_INTERRUPT, "TMS9901 interrupt %d active\n", i);
            /*  Always return 1, hardwired into the TMS9900 */
            return 1;
        }
    }

    return -1;
}

bool tms9901Interrupt (int index, uint8_t state)
{
    /*  Generate an interrupt for the each cru bit
     *  that is set (active low) and not masked.
     */
    int i;
    bool raise = false;

    /*  Interrupts to the TMS9900 are hardwired to always generate interrupt
     *  level 1 regardless of what device generated the interrupt.  So if any
     *  bit is low, we raise interrupt level 1, otherwise lower it.
     */
    for (i = 1; i < 16; i++)
        if (!cruBitGet (0, i) && !tms9901.intDisabled[i])
        {
            mprintf (LVL_INTERRUPT, "IRQ bit %d is low and enabled, raise interrupt\n", i);
            raise = true;
        }

    if (raise)
        tms9901.intActive[1] = 1;
    else
    {
        // mprintf (LVL_INTERRUPT, "IRQ bits all high or disabled, no interrupt pending\n");
        tms9901.intActive[1] = 0;
    }

    /*  Allow the bit state to be changed */
    return false;
}

/*  Timer increments is 1B * interval * 64 / 3M (system clock 3MHz)
 *  so to convert to nanoseconds, its 1000 * 64 * timer / 3
 */
int tms9901TimerNsec (void)
{
    return 1000 * 64 * tms9901.timer / 3;
}

void tms9901Init(void)
{
    int i;

    for (i = 0; i < 16; i++)
        tms9901.intActive[i] = 0;
}

static void timerCallback (void)
{
    if (tms9901.intDisabled[IRQ_TIMER])
    {
        mprintf (LVL_INTERRUPT, "TMS9901 timer expired, interrupt is disabled\n");
    }
    else
    {
        mprintf (LVL_INTERRUPT, "TMS9901 timer expired, raise interrupt\n");

        cruBitInput (0, IRQ_TIMER, 0);
        soundModulation (tms9901TimerNsec ());
    }
}

/*  Return true because the value should not be stored */
bool tms9901BitSet (int index, uint8_t state)
{
    if (tms9901.timerMode)
    {
        int bit = 1 << (index - 1);
        tms9901.timer &= ~bit;

        if (state)
            tms9901.timer |= bit;

        mprintf (LVL_INTERRUPT, "TMS9901 timer bit %d set to %d, timer set to %d\n", index, state, tms9901.timer);

        int nsec = tms9901TimerNsec ();
        timerStart (TIMER_TMS9901, nsec, timerCallback);
        printf ("t-start %d nsec\n", nsec);

        /* Don't allow the actual state of the bit to change */
        return true;
    }
    else
    {
        /* Interrupt mode.  Writing 1 enables an interrupt, 0 disables it */
        mprintf (LVL_INTERRUPT, "TMS9901 bit %d state %d interrupt is %s\n", index, state,
                (state == 0) ? "disabled" : "enabled");
        // printf ("[%d-%s]", index, state?"en":"dis");
        tms9901.intDisabled[index] = (state == 0);

        /* Set the input bit back to 1 to clear the interrupt condition.  This
         * doesn't seem to quite follow the TMS9901 documentaton, but is the
         * way the console ROM uses it
         */
        if (state != 0)
        {
            tms9901.intActive[index] = 0;
            cruBitInput (0, index, state);
        }

        /* TI console disables IRQ 3 when finished with cassette, but doesn't
         * stop the timer.  Since we use timers to control execution rate, we
         * stop the timer whenever we see the int disabled
         */
        if (index == IRQ_TIMER && state == 0)
            timerStop (TIMER_TMS9901);
    }

    return true;
}

/*  Reading CRU bits 1 thru 14.  If in timer mode, read the timer, otherwise
 *  return the requested state.
 */

uint8_t tms9901BitGet (int index, uint8_t state)
{
    /*  If we are not in timer mode, then return the value of the bit as is */
    if (tms9901.timerMode)
    {
        int bit = 1 << (index - 1);

        mprintf (LVL_INTERRUPT, "TMS9901 timer %d, return bit %d as %d\n", tms9901.timer, bit,
                tms9901.timer&bit);
        return (tms9901.timer & bit) ? 1 : 0;
    }

    /*  Otherwise return the state of the input */
    return state;
}

/*  Write to bit 0 sets the mode.  Mode 0 is input/IRQ mode, mode 1 is timer
 *  mode
 */
bool tms9901ModeSet (int index, uint8_t state)
{
    /*  Take a snapshot of the current clock timer on entering timer mode.  TODO
     *  this may have to read remaing time on a timerfd.
     */
    if (!tms9901.timerMode && state)
        tms9901.timerSnapshot = tms9901.timer;

    tms9901.timerMode = state ? true : false;
    mprintf (LVL_INTERRUPT, "TMS9901 timer mode set to %d\n", tms9901.timerMode ? 1 : 0);
    return false;
}

