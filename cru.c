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
#include <time.h>

#include "types.h"
#include "trace.h"
#include "cru.h"

#define MAX_CRU_BIT 4096

/*  Maintains state of the CRU bits and call callbacks as required when bits
 *  change.  Some bits have different behaviours when input vs output.  So we
 *  implement two callbacks, one for each.  An input is when an external entity
 *  modifies a bit (e.g. keyboard, VDP interrupt) where an output is when
 *  software changes a bit (SBO, SBZ, LDCR).  There is also a read callback as
 *  some bits need emulated software to generate their state (e.g. timer
 *  countdown value).
 *
 *  The state reflects any active hardware input but the value read by software
 *  may be different.  e.g. tms9901 timer read.  Also, writes by software may or
 *  may not modify the state.  e.g. bit 3 set will not affect the input to bit
 *  3.
 */
struct _cru
{
    uint8_t state;

    /*  All pins power up as inputs.  Outputting a value to a pin changes it to
     *  an output until the next reset
     */
    bool isOutput;
    /*  Define an input callback to be called when an entity external to the CPU
     *  modifies a CRU bit.  The callback should return true if the change is to
     *  be accepted.
     */
    bool (*inputCallback) (int index, uint8_t state);

    /*  Define a callback to be called when software tries to modify a bit.  The
     *  callback should return true if it has absorbed the change and the bit
     *  should not be modified or false is the change should be stored.
     */
    bool (*outputCallback) (int index, uint8_t state);

    /*  Define a read callback that is called when software reads a bit.  The
     *  current state of the bit is passed as a parameter. The read function may
     *  accept this value and return it, or over-ride it with its own value and
     *  return that instead.
     */
    uint8_t (*readCallback) (int index, uint8_t state);
}
cru[MAX_CRU_BIT];

/*  Input bits are set by an external actuator, e.g. keyboard */
void cruBitInput (uint16_t base, int8_t bitOffset, uint8_t state)
{
    uint16_t index;

    index = base / 2 + bitOffset;
    mprintf (LVL_CRU, "CRU: bit %d (>%04X) set to %d\n", index, index<<1, state);

    if (index >= MAX_CRU_BIT)
    {
        printf ("Attempt to set bit %d\n", index);
        halt ("out of range CRU");
    }

    struct _cru *c = &cru[index];

    c->state = state;

    if (c->inputCallback)
    {
        c->inputCallback (index, state);
    }
}

/*
 *  Used by SBO and SBZ.  Base is R12
 */
void cruBitOutput (uint16_t base, int8_t bitOffset, uint8_t state)
{
    uint16_t index;

    index = base / 2 + bitOffset;

    if (index >= MAX_CRU_BIT)
    {
        printf ("Attempt to set bit %d\n", index);
        halt ("out of range CRU");
    }

    struct _cru *c = &cru[index];

    c->isOutput = true;

    /*  If there is no output callback or if the output callback returns false
     *  to indicate the value should be stored, then store the value
     */
    if (!c->outputCallback || !c->outputCallback (index, state))
        c->state = state;

    mprintf (LVL_CRU, "CRU: bit %d (>%04X) set to %d\n", index, index<<1, c->state);
}

/*
 *  Used by TB.  Base is R12
 */
uint8_t cruBitGet (uint16_t base, int8_t bitOffset)
{
    uint16_t index;

    index = base / 2 + bitOffset;

    if (index >= MAX_CRU_BIT)
    {
        printf ("Attempt to get bit %d\n", index);
        halt ("out of range CRU");
    }

    struct _cru *c = &cru[index];

    if (c->readCallback)
        c->state = c->readCallback (index, c->state);

    mprintf (LVL_CRU, "CRU: bit %d (>%04X) get as %d\n", index, index<<1,
                     c->state);

    return c->state;
}

/*
 *  Used by LDCR.  Base is R12
 */
void cruMultiBitSet (uint16_t base, uint16_t data, int nBits)
{
    int i;

    /* "if nbits is 1 through 8, then bits are taken from the most significant
     * byte of the register. If nbits is 0, it is understood as 16 and the whole
     * register is transfered."
    */
    if (!nBits)
        nBits = 16;

    mprintf(LVL_CRU, "CRU multi set base=%04X data=%04X n=%d\n", base, data, nBits);
    for (i = 0; i < nBits; i++)
    {
        cruBitOutput (base, i, (data & (1<<i)) ? 1 : 0);
    }
}

/*
 *  Used by STCR.  Base is R12
 */
uint16_t cruMultiBitGet (uint16_t base, int nBits)
{
    int i;
    uint16_t data = 0;

    if (!nBits)
        nBits = 16;

    mprintf(LVL_CRU, "CRU start multi get base=>%04X n=%d\n", base, nBits);
    for (i = 0; i < nBits; i++)
    {
        if (cruBitGet (base, i))
        {
            data |= (1 << i);
        }
    }

    mprintf(LVL_CRU, "CRU multi get base=>%04X d=%x n=%d\n", base, data, nBits);
    return data;
}

void cruInputCallbackSet (int index, bool (*callback) (int index, uint8_t state))
{
    cru[index].inputCallback = callback;
}

void cruOutputCallbackSet (int index, bool (*callback) (int index, uint8_t state))
{
    cru[index].outputCallback = callback;
}

void cruReadCallbackSet (int index, uint8_t (*callback) (int index, uint8_t state))
{
    cru[index].readCallback = callback;
}

