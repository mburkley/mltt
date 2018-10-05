#include <stdio.h>

#include "cond.h"
#include "mem.h"

struct
{
    WORD    addr[20];
    WORD    value[20];
    int     count;
}
c;

void conditionAdd (char *s)
{
    int         i;
    int         a;
    int         v;

    if (sscanf (s, " %X %X", &a, &v) != 2)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < c.count; i++)
    {
        if (c.addr[i] == a)
        {
            printf ("*** Duplicate '%s'\n", s);
            return;
        }
    }

    if (c.count == 20)
    {
        printf ("*** Can't add '%s'\n", s);
        return;
    }

    c.addr[c.count] = a;
    c.value[c.count] = v;
    c.count++;
}

void conditionList (void)
{
    int         i;

    printf ("Conditions\n");
    printf ("==========\n");

    for (i = 0; i < c.count; i++)
    {
        printf ("#%2d : %04X = %04X\n", i, c.addr[i], c.value[i]);
    }
}

void conditionRemove (char *s)
{
    int         i;
    int         a;

    if (sscanf (s, "%X", &a) != 1)
    {
        printf ("*** Can't parse '%s'\n", s);
        return;
    }

    for (i = 0; i < c.count; i++)
    {
        if (c.addr[i] == a)
        {
            break;
        }
    }

    if (i == c.count)
    {
        printf ("*** Not found '%s'\n", s);
        return;
    }

    for (; i < c.count - 1; i++)
    {
        c.addr[i] = c.addr[i+1];
        c.value[i] = c.value[i+1];
    }

    c.count--;
}

int conditionTrue (int i)
{
    if (memRead(c.addr[i]) == c.value[i])
        return 1;

    return 0;
}

