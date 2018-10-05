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

void breakPointAdd (char *s)
{
    int         i;
    int         a;

    if (sscanf (s, "%X", &a) != 1)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == a)
        {
            printf ("*** Duplicate '%s'\n", s);
            return;
        }
    }

    if (bp.count == 20)
    {
        printf ("*** Can't add '%s'\n", s);
        return;
    }

    bp.breaks[bp.count] = a;
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

void breakPointRemove (char *s)
{
    int         i;
    int         a;

    if (sscanf (s, "%X", &a) != 1)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == a)
        {
            break;
        }
    }

    if (i == bp.count)
    {
        printf ("*** Not found '%s'\n", s);
        return;
    }

    for (; i < bp.count - 1; i++)
    {
        bp.breaks[i] = bp.breaks[i+1];
    }

    bp.count--;
}

void breakPointCondition (char *s)
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

int breakPointHit (WORD addr)
{
    int         i, j;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == addr)
        {
            for (j = 0; j < bp.condCount[i]; j++)
            {
                if (!conditionTrue (bp.conditions[i][j]))
                    return 0;
            }

            return 1;
        }
    }

    return 0;
}

