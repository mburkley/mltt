#include <stdio.h>
#include <time.h>
#include <dos.h>

#include "cru.h"

#define MAX_BIT 4096

struct
{
    BYTE state[MAX_BIT];
}
cru;

#ifdef __UNIT_TEST

#include <stdarg.h>

static int cruPrintf (char *s, ...)
{
    va_list ap;

    va_start (ap, s);
    vprintf (s, ap);
    va_end (ap);

    return 0;
}

#else

static int cruPrintf (char *s, ...)
{
    s=s;
    return 0;
}

#endif

/*
 *  Used by SBO and SBZ.  Base is R12
 */

void cruBitSet (WORD base, I8 bitOffset, BYTE state)
{
    WORD index;

    index = base / 2;
    index += bitOffset;

    // printf ("CRU: bit %04X set to %s\n", index, state ? "ON" : "OFF");

    if (index >= MAX_BIT)
    {
        printf ("Attempt to set bit %d\n", index);
        halt ("out of range CRU");
    }

    cru.state[index] = state;
}
 
/*
 *  Used by TB.  Base is R12
 */

BYTE cruBitGet (WORD base, I8 bitOffset)
{
    WORD index;

    index = base / 2;
    index += bitOffset;

    if (index >= MAX_BIT)
    {
        printf ("Attempt to get bit %d\n", index);
        halt ("out of range CRU");
    }

    if (index >= 3 && index <= 18)
    {
        // printf ("CRU: bit %04X get as ON (keyboard fudge)\n", index);
        return 1;
    }

    // printf ("CRU: bit %04X get as %s\n", index,
    //             cru.state[index] ? "ON" : "OFF");

    return cru.state[index];
}
 
/*
 *  Used by LDCR.  Base is R12
 */

void cruMultiBitSet (WORD base, WORD data, int nBits)
{
    int i;

    if (!nBits)
        nBits = 16;

    for (i = 0; i < nBits; i++)
    {
        cruBitSet (base, i, (data & 0x8000) ? 1 : 0);
        data <<= 1;
    }
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

    for (i = 0; i < nBits; i++)
    {
        if (cruBitGet (base, i))
        {
            data |= (0x8000 >> i);
        }
    }

    return data;
}

/*
updateKeyBoard ()
{
    if (!kbhit())
        return;

    switch (getch())
    {
        case '=':
        case

*/

#ifdef __UNIT_TEST

int main (void)
{
    printf ("Set bit 0 to 1\n");
    cruBitSet (0, 0, 1);

    printf ("Set bit 0x104 to 1\n");
    cruBitSet (0x200, 0x4, 1);

    printf ("Set bit 0xFC to 1\n");
    cruBitSet (0x200, -0x4, 1);

    printf ("Get bit 0xFC\n");
    if (!cruBitGet (0x1F8, 0))
        printf ("ERROR: bit not set\n");

    printf ("Get bit 0x100\n");
    if (cruBitGet (0x200, 0))
        printf ("ERROR: bit set\n");

    printf ("Get bit 0x104 positive rel\n");
    if (!cruBitGet (0x200, 4))
        printf ("ERROR: bit not set\n");

    printf ("Get bit 0x104 negative rel\n");
    if (!cruBitGet (0x210, -4))
        printf ("ERROR: bit not set\n");
}

#endif

