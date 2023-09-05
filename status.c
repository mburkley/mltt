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

/*  Implement a status pane alongside the rendered TI output */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include "vdp.h"
#include "grom.h"
#include "cpu.h"
#include "status.h"

/* Private copy of character data for status display */
static BYTE statusChars[96 * 7];
static bool statusPaneActive = false;

static struct _statusSpriteInfo
{
    int x;
    int y;
    int pat;
    int colour;
}
statusSpriteInfo[32];

static struct _statusSoundInfo
{
    int amplitude;
    int period;
}
statusSoundInfo[4];

static int statusPosX;
static int statusPosY;

static int statusPaneHeight;
static int statusPaneWidth;
static int statusPaneXOffset;

static void statusDrawChar (int cx, int cy, int ch)
{
    int i, j;
    int charpat = (ch & 0x7F) - 0x20;
    int x1 = (cx<<3)+statusPaneXOffset;
    int y1 = cy<<3;

    for (j = 0; j < 8; j++)
    {
        int data;

        /*  First row of char is always blank, other rows fetched from local
         *  pattern array.  Mul 7 as 7 rows per char, offset -1 since 0-8=>0-7
         */
        if (j == 0)
            data = 0;
        else
            data = statusChars[charpat*7+j-1];

        for (i = 0; i < 8; i++)
        {
            /*  Colour hard coded to white on blue */
            vdpPlotRaw (x1 + i, y1 + j, (data & 0x80) ? 0x0F : 0x04);
            data <<= 1;
        }
    }
}

static void statusClearToEol (void)
{
    while (statusPosX < statusPaneWidth >> 3)
        statusDrawChar (statusPosX++, statusPosY, ' ');

    statusPosY++;
    statusPosX = 0;
}

static void statusPrintf (char *fmt, ...)
{
    char line[1024];
    int i;
    va_list ap;

    va_start (ap, fmt);
    vsprintf (line, fmt, ap);
    va_end (ap);

    // printf("V:%s@%d,%d\n", s, vdpStatusPosX, vdpStatusPosY);
    for (i = 0; i <strlen (line); i++)
    {
        if (line[i] == '\n')
        {
            /*  Clear the remainder of the line */
            statusClearToEol ();
        }
        else if (line[i] == '\f')
        {
            /*  Clear the remainder of the form / status pane */
            while (statusPosY < statusPaneHeight >> 3)
                statusClearToEol ();
        }
        else
        {
            #if 0

            statusDrawCharRaw (statusPosX + 32, statusPosY,
                            &statusChars[charpat*7], 7, 0xf1);
            #endif

            statusDrawChar (statusPosX++, statusPosY, line[i]);

            if (statusPosX >= statusPaneWidth >> 3)
            {
                statusPosX = 0;
                statusPosY++;
            }
        }
    }
}

void statusPaneDisplay (void)
{
    int i;

    if (!statusPaneActive)
        return;

    statusPosX = statusPosY = 0;

    #if 0
    statusPrintf("VDP:\n");

    for (i = 0; i < 8; i++)
    {
        vdpPrintf ("  R%d:%02X\n", i, vdp.reg[i]);
    }

    statusPrintf ("  St:%02X\n", status.st);
    #endif

    statusPrintf("\nCPU:\n");
    statusPrintf("  PC:%04X\n", cpuGetPC());
    statusPrintf("  WP:%04X\n", cpuGetWP());
    statusPrintf("  ST:%04X\n", cpuGetST());

    statusPrintf("\nGROM:\n");
    statusPrintf("  PC:%04X\n", gromAddr());

    statusPrintf ("\nSprites:\n");

    for (i = 0; i < 32; i++)
    {
        struct _statusSpriteInfo *s = &statusSpriteInfo[i];

        if (s->colour == 0)
            continue;

        statusPrintf ("  %2d @ %d,%d p=%X c=%X\n",
                      i, s->x, s->y, s->pat, s->colour);
    }

    statusPrintf ("\nSound:\n");

    for (i = 0; i < 4; i++)
    {
        struct _statusSoundInfo *s = &statusSoundInfo[i];

        statusPrintf ("  %2d amp=%2d per=%d\n", i, s->amplitude, s->period);
    }

    statusPrintf ("\f");
}

void statusSpriteUpdate (int index, int x, int y, int pat, int colour)
{
    struct _statusSpriteInfo *s = &statusSpriteInfo[index];
    s->x = x;
    s->y = y;
    s->pat = pat;
    s->colour = colour;
}

void statusSoundUpdate (int index, int amplitude, int period)
{
    struct _statusSoundInfo *s = &statusSoundInfo[index];
    s->amplitude = amplitude;
    s->period = period;
}

void statusPaneInit (int width, int height, int xOffset)
{
    statusPaneHeight = height;
    statusPaneWidth = width;
    statusPaneXOffset = xOffset;

    /*  To have a consistent status pane update we take a local copy of the
     *  character table in GROM and use that to display status characters.  The
     *  GROM small font is stored as 7 bytes per char (last byte is a always 0
     *  as matrix is 7x5).  There are 96 chars stored starting at grom:0x6B4
     */
    for (int i = 0; i < 96 * 7; i++)
        statusChars[i] = gromData (i + 0x6B4);

    statusPaneActive = true;
}

