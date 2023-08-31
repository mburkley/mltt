/*
 *  Manage breakpoints in the execution of the machine code
 */

#include <stdio.h>

#include "cond.h"
#include "break.h"

struct
{
    WORD    breaks[20];
    int     conditions[20][5];
    int     condCount[20];
    int     count;
}
bp;

void breakPointAdd (WORD addr)
{
    int         i;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == addr)
        {
            printf ("*** Duplicate '%04X'\n", addr);
            return;
        }
    }

    if (bp.count == 20)
    {
        printf ("*** Can't add '%04X'\n", addr);
        return;
    }

    bp.breaks[bp.count] = addr;
    bp.count++;
}

void breakPointList (void)
{
    int         i, j;

    printf ("Break Points\n");
    printf ("============\n");

    for (i = 0; i < bp.count; i++)
    {
        printf ("#%2d (PC = %04X)", i, bp.breaks[i]);

        for (j = 0; j < bp.condCount[i]; j++)
            printf (" && (cond # %d)",
                    bp.conditions[i][j]);

        printf ("\n");
    }
}

void breakPointRemove (WORD addr)
{
    int         i;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == addr)
        {
            break;
        }
    }

    if (i == bp.count)
    {
        printf ("*** Not found '%04X'\n", addr);
        return;
    }

    for (; i < bp.count - 1; i++)
    {
        bp.breaks[i] = bp.breaks[i+1];
    }

    bp.count--;
}

#if 0
void breakPointCondition (WORD addr
{
    int b, c;
    int i;

    if (sscanf (s, "%d %d", &b, &c) != 2)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < bp.condCount[b]; i++)
    {
        if (bp.conditions[b][i] == c)
        {
            printf ("*** Duplicate '%s'\n", s);
            return;
        }
    }

    if (bp.condCount[b] == 5)
    {
        printf ("*** Can't add '%s'\n", s);
        return;
    }

    bp.conditions[b][bp.condCount[b]] = c;
    bp.condCount[b]++;

}
#endif

int breakPointHit (WORD addr)
{
    int         i; // , j;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == addr)
        {
            return 1;
        }
    }

    return 0;
}

