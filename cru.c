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

#include "trace.h"
#include "cru.h"

#define MAX_CRU_BIT 4096

struct
{
    BYTE state;
    void (*callback) (int index, BYTE state);
    int isInput;
    int modified;
}
cru[MAX_CRU_BIT];

static void bitSet (int index, BYTE state)
{
    if (index != 2)
        mprintf (LVL_CRU, "CRU: bit %04X set to %s\n", index<<1, state ? "ON" : "OFF");

    mprintf (LVL_CRU, "CRU: bit %04X set to %s\n", index<<1, state ? "ON" : "OFF");

    if (index >= MAX_CRU_BIT)
    {
        printf ("Attempt to set bit %d\n", index);
        halt ("out of range CRU");
    }

    cru[index].state = state;
    cru[index].modified = 1;

    if (cru[index].callback)
    {
        mprintf (LVL_CRU, "CRU: callback for bit %d\n", index);
        cru[index].callback (index, state);
        mprintf (LVL_CRU, "CRU: callback return for bit %d\n", index);
    }
}

void cruBitInput (WORD base, I8 bitOffset, BYTE state)
{
    WORD index;

    index = base / 2;
    index += bitOffset;

    cru[index].isInput = 1;
    bitSet (index, state);
}

/*
 *  Used by SBO and SBZ.  Base is R12
 */
void cruBitSet (WORD base, I8 bitOffset, BYTE state)
{
    WORD index;

    index = base / 2;
    index += bitOffset;

    if (cru[index].isInput)
    {
        mprintf (LVL_CRU, "CRU set input bit ignored\n");
        return;
    }

    bitSet (index, state);
}

/*
 *  Used by TB.  Base is R12
 */
BYTE cruBitGet (WORD base, I8 bitOffset)
{
    WORD index;

    index = base / 2;
    index += bitOffset;

    if (index >= MAX_CRU_BIT)
    {
        printf ("Attempt to get bit %d\n", index);
        halt ("out of range CRU");
    }

    if (cru[index].modified)
        mprintf (LVL_CRU, "CRU: bit %04X get as %s\n", index<<1,
                     cru[index].state ? "ON" : "OFF");

    cru[index].modified = 0;
    return cru[index].state;
}

/*
 *  Used by LDCR.  Base is R12
 */
void cruMultiBitSet (WORD base, WORD data, int nBits)
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
        cruBitSet (base, i, (data & (1<<i)) ? 1 : 0);
    }
    mprintf(LVL_CRU, "CRU multi set done\n");
}

/*
 *  Used by STCR.  Base is R12
 */
WORD cruMultiBitGet (WORD base, int nBits)
{
    int i;
    WORD data = 0;

    if (!nBits)
        nBits = 16;

    mprintf(LVL_CRU, "CRU start multi get b=%x n=%d\n", base, nBits);
    for (i = 0; i < nBits; i++)
    {
        if (cruBitGet (base, i))
        {
            data |= (1 << i);
        }
    }

    mprintf(LVL_CRU, "CRU multi get b=%x d=%x n=%d\n", base, data, nBits);
    return data;
}

void cruCallbackSet (int index, void (*callback) (int index, BYTE state))
{
    cru[index].callback = callback;
}

