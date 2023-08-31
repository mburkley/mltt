#include <stdio.h>

#include "break.h"
#include "mem.h"

struct
{
    WORD    last[20];
    WORD    addr[20];
    int     count;
}
w;

void watchAdd (WORD addr)
{
    int         i;
    // int         a;

    for (i = 0; i < w.count; i++)
    {
        if (w.addr[i] == addr)
        {
            printf ("*** Duplicate '%04X'\n", addr);
            return;
        }
    }

    if (w.count == 20)
    {
        printf ("*** Can't add '%04X'\n", addr);
        return;
    }

    w.addr[w.count] = addr;
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

void watchRemove (WORD addr)
{
    int         i;
    // int         a;

    for (i = 0; i < w.count; i++)
    {
        if (w.addr[i] == addr)
        {
            break;
        }
    }

    if (i == w.count)
    {
        printf ("*** Not found '%04X'\n", addr);
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
        data = memReadW (w.addr[i]);

        if (w.last[i] != data)
        {
            printf ("W%d : [%04X] -> %04X\n",
                    i, w.addr[i], data);
            w.last[i] = data;
        }
    }
}

