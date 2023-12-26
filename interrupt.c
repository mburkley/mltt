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

#include "types.h"
#include "trace.h"
#include "cru.h"
#include "interrupt.h"
#include "timer.h"
#include "cassette.h"

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
    int level = -1;

    /*  Interrupts to the TMS9900 are hardwired to always generate interrupt
     *  level 1 regardless of what device generated the interrupt.  So if any
     *  level is > 0 and mask > 0, return level 1.
     */

    if (mask == 0)
        return -1;

    for (i = 0; i <= 16; i++)
    {
        if (tms9901.intActive[i])
        {
            mprintf (LVL_INTERRUPT, "TMS9901 interrupt %d active\n", i);
            level = i;
            break;
        }
    }

    if (level >= 0)
        return 1;

    return -1;
}

bool tms9901Interrupt (int index, uint8_t state)
{
    /*  Set interrupt active for the each cru bit
     *  that is active low and not masked.
     */
    if (!state && !tms9901.intDisabled[index])
    {
        mprintf (LVL_INTERRUPT, "IRQ bit %d is low and enabled, raise interrupt\n", index);
        tms9901.intActive[index] = 1;
    }
    else
    {
        tms9901.intActive[index] = 0;
    }

    /*  Allow the bit state to be changed */
    return false;
}

/*  Timer increments is 1B * interval * 64 / 3M (system clock 3MHz)
 *  so to convert to nanoseconds, its 1000 * 64 * timer / 3
 */
int tms9901TimerToNsec (void)
{
    return 1000 * 64 * tms9901.timer / 3;
}

/*  Do the reverse of the above conversion to convert a remaining value in
 *  nanseconds to a timer register value
 */
int tms9901TimerFromNsec (int value)
{
    return value * 3 / (1000 * 64);
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
        /*  TODO this is not a good dependency.  9901 should know nothing about
         *  cassettes
         */
        cassetteTimerExpired (tms9901TimerToNsec ());
    }
}

/*  Return true because the value should not be stored */
bool tms9901BitSet (int index, uint8_t state)
{
    if (tms9901.timerMode)
    {
        int bit = 1 << (index - 1);
        int newTimerValue = 0;

        newTimerValue = tms9901.timer & ~bit;

        if (state)
            newTimerValue |= bit;

        if (newTimerValue != tms9901.timer)
            mprintf (LVL_INTERRUPT, "TMS9901 timer bit %d set to %d, timer set to %d\n", index, state, tms9901.timer);

        tms9901.timer = newTimerValue;
        int nsec = tms9901TimerToNsec ();
        timerStart (TIMER_TMS9901, nsec, timerCallback);
        mprintf (LVL_INTERRUPT, "t-start %04X -> %d nsec\n", tms9901.timer, nsec);
        int timer = tms9901TimerFromNsec (timerRemain (TIMER_TMS9901));
        mprintf (LVL_INTERRUPT, "TMS9901 timer remain %04X\n", timer);

        /* Don't allow the actual state of the bit to change */
        return true;
    }
    else
    {
        /* Interrupt mode.  Writing 1 enables an interrupt, 0 disables it */
        mprintf (LVL_INTERRUPT, "TMS9901 bit %d state %d interrupt is %s\n", index, state,
                (state == 0) ? "disabled" : "enabled");
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
        {
            mprintf (LVL_INTERRUPT, "TMS9901 timer stopped\n");
            timerStop (TIMER_TMS9901);
        }
    }

    return true;
}

/*  Reading CRU bits 1 thru 14.  If in timer mode, read the timer, otherwise
 *  return the requested state.
 */

uint8_t tms9901BitGet (int index, uint8_t state)
{
    /*  If we are in timer mode, then return the bit value of the remaining
     *  timer bit */
    if (tms9901.timerMode)
    {
        int timer = tms9901TimerFromNsec (timerRemain (TIMER_TMS9901));
        int bit = 1 << (index - 1);

        mprintf (LVL_INTERRUPT, "TMS9901 timer remain %04X, return bit %d as %d\n", timer, bit,
                timer&bit);
        return (timer & bit) ? 1 : 0;
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

    if (tms9901.timerMode)
        mprintf (LVL_INTERRUPT, "TMS9901 clock mode set\n");
    else
        mprintf (LVL_INTERRUPT, "TMS9901 interrupt mode set\n");

    return false;
}

