#include "break.h"

struct
{
    WORD    breaks[20];
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
    int         i;

    printf ("Break Points\n");
    printf ("============\n");

    for (i = 0; i < bp.count; i++)
    {
        printf ("#%2d - %04X\n", i, bp.breaks[i]);
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

int breakPointHit (WORD addr)
{
    int         i;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.breaks[i] == addr)
        {
            return 1;
        }
    }

    return 0;
}

