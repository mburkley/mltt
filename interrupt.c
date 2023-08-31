/*
 *  Implements TMS9901 interrupt generation.  Even though the TI994/A is
 *  hardwired to generate only one interrupt level, we implement 16 for
 *  completeness.
 */

#include <stdio.h>

#include "trace.h"

static struct
{
    int intActive[16];
}
tms9901;

/*  Set an interrupt request level */
void interruptRequest (int level)
{
    // if (!tms9901.intActive[level])
    //     printf("Interrupt level %d\n", level);

    tms9901.intActive[level] = 1;
}

/*  Return the highest priority interrupt pending */
int interruptLevel (int mask)
{
    int i;

    for (i = 0; i <= mask; i++)
    {
        if (tms9901.intActive[i])
        {
            // printf("int reset level %d\n", i);
            tms9901.intActive[i] = 0; // TODO ?
            return i;
        }
    }

    return -1;
}
    

    #if 0
    if (tms9900.irq <= mask)
    {
        // mprintf (LVL_CPU, "interrupt pending %d<%d\n", tms9900.irq, mask);
        printf ("interrupt pending %d<%d\n", tms9900.irq, mask);
        mask--;
        tms9900.st = (tms9900.st & ~FLAG_MSK) | mask;
        else
            mprintf (LVL_CPU, "unknown?\n");
    }
    #endif


void interruptInit(void)
{
    int i;

    for (i = 0; i < 16; i++)
        tms9901.intActive[i] = 0;
}

