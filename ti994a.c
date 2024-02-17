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
 *  Implements the console specifics, I/O, etc, of the 99-4a
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "types.h"
#include "cpu.h"
#include "mem.h"
#include "sound.h"
#include "speech.h"
#include "grom.h"
#include "vdp.h"
#include "trace.h"
#include "break.h"
#include "watch.h"
#include "cond.h"
#include "unasm.h"
#include "kbd.h"
#include "cru.h"
#include "timer.h"
#include "interrupt.h"
#include "gpl.h"
#include "cassette.h"
#include "fdd.h"
#include "ti994a.h"

bool ti994aRunFlag;

/*  Show the contents of the scratchpad memory either in abbreviated or detailed
 *  form.  If the param is false, just a hex dump is shown.  If true, each value
 *  is presented on a separate line with a description
 */
void ti994aShowScratchPad (bool showGplUsage)
{
    printf ("Scratchpad\n");
    printf ("==========");

    if (showGplUsage)
    {
        uint16_t addr = 0x8300;

        while (addr >= 0x8300 && addr < 0x8400)
        {
            uint16_t nextAddr = gplScratchPadNext (addr);
            memPrintScratchMemory (addr - 0x8300, nextAddr - addr);
            gplShowScratchPad(addr);
            addr = nextAddr;
        }
    }
    else
        memPrintScratchMemory (0, 0x100);

    printf ("\n");
}

void ti994aVideoInterrupt (void)
{
    vdpRefresh(0);

    /*
     *  Clear bit 2 to indicate VDP interrupt
     */
    mprintf (LVL_INTERRUPT, "IRQ_VDP lowered\n");
    cruBitInput (0, IRQ_VDP, 0);
}

void ti994aRun (int instPerInterrupt)
{
    static int count;

    ti994aRunFlag = true;
    printf("enter run loop\n");

    while (ti994aRunFlag &&
           !breakPointHit (cpuGetPC()) &&
           !conditionTrue ())
    {
        uint16_t opcode = cpuFetch ();
        bool shouldBlock = false;

        /*  Check if the instruction we are about to execute is >10FF, which is
         *  an infinite loop.  It is used to wait for an interrupt during
         *  cassette operations.  There is no need to actually spin, just go
         *  straight to a blocking read on the interrupt timer.
         */
        if (opcode == 0x10FF)
        {
            shouldBlock = true;
        }

        /*  To approximate execution speed at around 10 clock cycles per
         *  instruction with a 3MHz clock, we expect to execute about 2000
         *  instructions per VDP interrupt so do a blocking timer read once
         *  count reaches this value.
         *
         *  TODO : trick, LIMI 2 is the main GPL loop but won't mess up cassette
         *  ops.  Will this work with munchman and other ROM games?
         *
         *  Messes up frogger, TODO
         */
        if (cpuGetIntMask() == 2 && count >= instPerInterrupt)
        {
            shouldBlock = true;
            count -= instPerInterrupt;
        }

        if (shouldBlock)
        {
            kbdPoll ();
            timerPoll ();
        }

        cpuExecute (opcode);
        count++;

        watchShow();
    }

    ti994aRunFlag = false;
}

void ti994aInit (void)
{
    tms9901Init ();
    timerInit ();

    /*  Start a 20-msec (20,000,000 nanosec == 50Hz) recurring timer to generate video interrupts */
    timerStart (TIMER_VDP, 20000000, ti994aVideoInterrupt);

    int i;

    /*  Attach handler to bit 0 to set mode */
    cruOutputCallbackSet (0, tms9901ModeSet);

    /*  Attach handlers to CRU bits that generate interrupts (input), mask
     *  interrupts (output in interrupt mode) or set the clock (output in timer
     *  mode).
     */
    for (i = 1; i <= 15; i++)
    {
        cruBitInput (0, i, 1);  // Initial state inactive (active low)
        cruInputCallbackSet (i, tms9901Interrupt);
        cruOutputCallbackSet (i, tms9901BitSet);
        cruReadCallbackSet (i, tms9901BitGet);
    }

    /*  Attach handlers to CRU bits that are for keyboard input column select.
     */
    for (i = 18; i <= 20; i++)
        cruOutputCallbackSet (i, kbdColumnUpdate);

    cruOutputCallbackSet (22, cassetteMotor);
    cruOutputCallbackSet (23, cassetteMotor);
    cruOutputCallbackSet (24, cassetteAudioGate);
    cruOutputCallbackSet (25, cassetteTapeOutput);
    cruReadCallbackSet (27, cassetteTapeInput);
}

void ti994aClose (void)
{
    timerClose ();
}

