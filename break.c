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

/*
 *  Manage breakpoints in the execution of the machine code.
 */

#include <stdio.h>

#include "cond.h"
#include "break.h"

#define MAX_BREAKPOINTS 20
#define MAX_CONDITIONS   5

struct
{
    WORD    addr[MAX_BREAKPOINTS];
    int     count;
}
bp;

void breakPointAdd (WORD addr)
{
    int         i;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.addr[i] == addr)
        {
            printf ("*** Duplicate '%04X'\n", addr);
            return;
        }
    }

    if (bp.count == MAX_BREAKPOINTS)
    {
        printf ("*** Can't add '%04X'\n", addr);
        return;
    }

    bp.addr[bp.count++] = addr;
}

void breakPointList (void)
{
    int         i;

    printf ("Break Points\n");
    printf ("============\n");

    for (i = 0; i < bp.count; i++)
    {
        printf ("#%2d (PC = %04X)\n", i, bp.addr[i]);
    }
}

void breakPointRemove (WORD addr)
{
    int         i;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.addr[i] == addr)
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
        bp.addr[i] = bp.addr[i+1];
    }

    bp.count--;
}

int breakPointHit (WORD addr)
{
    int         i;

    for (i = 0; i < bp.count; i++)
    {
        if (bp.addr[i] == addr)
        {
            return 1;
        }
    }

    return 0;
}

