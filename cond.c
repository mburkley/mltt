#include <stdio.h>
#include <string.h>

#include "cond.h"
#include "mem.h"

#define MAX 100

#define COND_EQUAL      1
#define COND_NEQUAL     2
#define COND_CHANGE     3

struct
{
    WORD    addr;
    WORD    value;
    int cond;
}
c[MAX];

static int condCount;

static int conditionEval (int i)
{
    int val = memReadW(c[i].addr);

    switch (c[i].cond)
    {
    case COND_EQUAL:
        if (val == c[i].value)
            return 1;
        break;

    case COND_NEQUAL:
        if (val != c[i].value)
            return 1;
        break;

    case COND_CHANGE:
        if (val != c[i].value)
        {
            c[i].value = val;
            return 1;
        }
        break;
    }

    return 0;
}

void conditionAdd (WORD addr, char *comp, WORD value)
{
    int         i;

    for (i = 0; i < condCount; i++)
    {
        if (c[i].addr == addr)
        {
            printf ("*** Duplicate '%04X'\n", addr);
            return;
        }
    }

    if (condCount == MAX)
    {
        printf ("*** Can't add '%04X'\n", addr);
        return;
    }

    if (!strcasecmp (comp, "eq"))
        c[i].cond = COND_EQUAL;
    else if (!strcasecmp (comp, "ne"))
        c[i].cond = COND_NEQUAL;
    else if (!strcasecmp (comp, "ch"))
        c[i].cond = COND_CHANGE;
    else
    {
        printf ("*** must be CH, EQ or NE '%s'\n", comp);
        return;
    }

    c[i].addr = addr;
    c[i].value = value;
    condCount++;
}

void conditionList (void)
{
    int         i;

    printf ("Conditions\n");
    printf ("==========\n");

    for (i = 0; i < condCount; i++)
    {
        printf ("#%2d : %04X %s %04X %s\n", i, c[i].addr,
        c[i].cond==COND_EQUAL?"==":"!=",c[i].value,
        conditionEval (i) ? "TRUE":"");
    }
}

void conditionRemove (WORD addr)
{
    int         i;
    // int         a;

    for (i = 0; i < condCount; i++)
    {
        if (c[i].addr == addr)
        {
            break;
        }
    }

    if (i == condCount)
    {
        printf ("*** Not found '%04X'\n", addr);
        return;
    }

    for (; i < condCount - 1; i++)
    {
        c[i].addr = c[i+1].addr;
        c[i].cond = c[i+1].cond;
        c[i].value = c[i+1].value;
    }

    condCount--;
}

int conditionTrue (void)
{
    int i;

    for (i = 0; i < condCount; i++)
        if (conditionEval (i))
            return 1;

    return 0;
}

