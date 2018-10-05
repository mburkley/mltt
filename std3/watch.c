#include <stdio.h>

#include "break.h"

struct
{
    WORD    last[20];
    WORD    addr[20];
    int     count;
}
w;

void watchAdd (char *s)
{
    int         i;
    int         a;

    if (sscanf (s, "%X", &a) != 1)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < w.count; i++)
    {
        if (w.addr[i] == a)
        {
            printf ("*** Duplicate '%s'\n", s);
            return;
        }
    }

    if (w.count == 20)
    {
        printf ("*** Can't add '%s'\n", s);
        return;
    }

    w.addr[w.count] = a;
    w.count++;
}

void watchList (void)
{
    int         i;

    printf ("Watched mem locs\n");
    printf ("================\n");

    for (i = 0; i < w.count; i++)
    {
        printf ("#%2d - %04X\n", i, w.addr[i]);
    }
}

void watchRemove (char *s)
{
    int         i;
    int         a;

    if (sscanf (s, "%X", &a) != 1)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < w.count; i++)
    {
        if (w.addr[i] == a)
        {
            break;
        }
    }

    if (i == w.count)
    {
        printf ("*** Not found '%s'\n", s);
        return;
    }

    for (; i < w.count - 1; i++)
    {
        w.addr[i] = w.addr[i+1];
    }

    w.count--;
}

void watchShow (void)
{
    int         i;
    int         data;

    for (i = 0; i < w.count; i++)
    {
        data = cpuRead (w.addr[i]);

        if (w.last[i] != data)
        {
            printf ("W%d : [%04X] -> %04X\n",
                    i, w.addr[i], data);
            w.last[i] = data;
        }
    }
}

