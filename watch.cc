/*
 * Copyright (c) 2004-2024 Mark Burkley.
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

#include <stdio.h>

#include "break.h"
#include "mem.h"

struct
{
    uint16_t    last[20];
    uint16_t    addr[20];
    int     count;
}
w;

void watchAdd (uint16_t addr)
{
    int         i;

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

void watchRemove (uint16_t addr)
{
    int         i;

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

