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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>

#include "trace.h"
#include "kbd.h"
#include "cru.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static int kbdFd;
static char kbdDevice[256];

/*
 *  This table is just used to create debug output.  It contains a string
 *  representation of the keys being pressed.
 */
static char *keyMap[KBD_ROW][KBD_COL] =
{
    { "=",    ".", ",", "m", "n", "/", 0, 0 }, // JS fire
    { " ",    "l", "k", "j", "h", ";", 0, 0 }, // JS left
    { "CR",   "o", "i", "u", "y", "p", 0, 0 }, // JS right
    { "",     "9", "8", "7", "6", "0", 0, 0 }, // JS down
    { "FNCT", "2", "3", "4", "5", "1", 0, 0 }, // JS up / alpha lock
    { "SHFT", "s", "d", "f", "g", "a", 0, 0 },
    { "CTRL", "w", "e", "r", "t", "q", 0, 0 },
    { "",     "x", "c", "v", "b", "z", 0, 0 }
};

/*
 *  This table maps key up/down events to the row/col matrix in the TI99/4a
 */
static int keyCode[KBD_ROW][KBD_COL] = 
{
    { 13, 52, 51, 50, 49, 53, 0, 0 },
    { 57, 38, 37, 36, 35, 39, 0, 0 },
    { 28, 24, 23, 22, 21, 25, 0, 0 },
    {  0, 10,  9,  8,  7, 11, 0, 0 },
    { 56, 3,  4,  5,  6,  2,  0, 0 }, // Left-Alt = FNCT
    { 42, 31, 32, 33, 34, 30, 0, 0 }, // Left-Shift = SHFT
    { 29, 17, 18, 19, 20, 16, 0, 0 }, // Left-Control = CTRL
    {  0, 45, 46, 47, 48, 44, 0, 0 }
};

/*  This is a start at building "virtual keys" where keys on a real keyboard
 *  create multi-key sequences.  e.g. left arrow key creates FNCT+S.
 */
#if 1
static struct _extended
{
    int code;
    int row1;
    int col1;
    int row2;
    int col2;
}
keyExtended[] =
{
//    { 14,  }, // Backspace => FNCT+???
    { 105, 4, 0, 5, 1},     // Left = FNCT+S
    { 106, 4, 0, 5, 2},     // Right = FNCT+D
    { 103, 4, 0, 6, 2},       // Up = FNCT+E
    { 108, 4, 0, 7, 1},      // Down = FNCT+X
    { 110, 4, 0, 4, 1},  // Ins = FNCT+2
    { 111, 4, 0, 4, 5}   // Del = FNCT+1
//    { 58 // caps lock
//    { 54,  // right shift
//    { 97 // right control
//    { 100 // altgr = alphalock?
};
#endif

/*  Maintain a current and previous table of key states.  The lastState table is
 *  only used to reduce debug output.
 */
static int keyState[KBD_ROW][KBD_COL];
static int lastState[KBD_ROW][KBD_COL];

static void kbdReopen (void)
{
    if (kbdFd != -1)
        close (kbdFd);

    kbdFd = open (kbdDevice, O_RDONLY);

    if (kbdFd == -1)
    {
        mprintf (LVL_KBD, "%s failed to open device %s error %s\n", __func__,
              kbdDevice, strerror (errno));
        halt("check keyboard input device");
    }
}

static void decodeEvent (struct input_event ev)
{
    int i, j;
    int mapped = 0;

    if (ev.type == EV_KEY && ev.value < 2)
    {
        for (i = 0; i < KBD_ROW; i++)
            for (j = 0; j < KBD_COL; j++)
                if (keyCode[i][j] == ev.code)
                {
                    mprintf (LVL_KBD, "%s KEY %s -> '%s' (%d,%d)\n",
                             __func__,
                             ev.value == 0 ? "UP" : "DOWN",
                             keyMap[i][j], i, j);
                    keyState[i][j] = ev.value;
                    mapped = 1;
                }

        if (!mapped)
        {
            for (i = 0; i < sizeof keyExtended / sizeof (struct _extended); i++)
            {
                struct _extended *e = &keyExtended[i];
                if (e->code == ev.code)
                {
                    keyState[e->row1][e->col1] = ev.value;
                    keyState[e->row2][e->col2] = ev.value;
                    mapped = 1;
                }
            }
        }

        if (!mapped)
            mprintf (LVL_KBD, "%s unknown KEY %s not mapped c=%d\n", __func__,
                   ev.value == 0 ? "UP" : "DOWN", ev.code);
    }
}

static int kbdColumn;

bool kbdColumnUpdate (int index, uint8_t value)
{
    if (index < 18 || index > 20)
    {
        printf ("bad KBD col CRU index %d\n", index);
        halt ("bad KBD col");
    }

    index -= 18;
    int bit = 1 << index;

    kbdColumn &= ~bit;

    if (value)
        kbdColumn |= bit;

    int kbdRow;

    mprintf (LVL_KBD, "KBD scan col %d\n", kbdColumn);

    for (kbdRow = 0; kbdRow < KBD_COL; kbdRow++)
    {
        int bit = keyState[kbdRow][kbdColumn] ? 0 : 1;

        if (keyState[kbdRow][kbdColumn] != lastState[kbdRow][kbdColumn])
        {
            mprintf (LVL_KBD, "%s scan kbdRow/kbdColumn %d/%d = %d\n", __func__, kbdRow, kbdColumn, keyState[kbdRow][kbdColumn]);
            lastState[kbdRow][kbdColumn] = keyState[kbdRow][kbdColumn];
        }


        if (!bit)
            mprintf (LVL_KBD, "KBD col %d, row %d active\n", kbdColumn, kbdRow);

        cruBitInput (0, 3+kbdRow, bit);
    }

    /*  Return false as we want this value to be stored */
    return false;
}

#define PROC_FILE "/proc/bus/input/devices"
#define HANDLERS "H: Handlers="
#define EVENTS "B: EV="

void kbdFindInputDevice (void)
{
    FILE *fp;
    unsigned events = 0;
    char handlers[256] = "";
    char s[256];
    bool found = false;

    if ((fp = fopen ("/proc/bus/input/devices", "r")) == NULL)
        halt ("Open /proc/bus/input/devices");

    while (!feof (fp))
    {
        if (fgets (s, sizeof s, fp) == NULL)
            break;

        if (!strncmp (s, HANDLERS, strlen (HANDLERS)))
            strcpy (handlers, s+strlen (HANDLERS));
        else if (!strncmp (s, EVENTS, strlen (EVENTS)))
            events=strtoul (s+strlen (EVENTS), NULL, 16);

        if ((events & 0x120013) == 0x120013)
        {
            break;
        }
    }

    fclose (fp);

    if (!events)
        halt ("no events");

    char *tok = handlers;

    tok = strtok (tok, " ");

    while (tok)
    {
        if (!strncmp (tok, "event", 5))
        {
            sprintf (kbdDevice, "/dev/input/%s", tok);
            printf ("Found kbd input dev %s\n", kbdDevice);
            found = true;
        }

        tok = strtok (NULL, " ");
    }

    if (!found)
        halt ("Can't find keyboard");
}

void kbdPoll (void)
{
    struct input_event ev;
    struct pollfd pfds[2];
    int n;
    int ret;

    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = kbdFd;
    pfds[1].events = POLLIN;
    ret = poll(pfds, 2, 0);

    if (ret < 1)
        return;

    if (!(pfds[1].revents & POLLIN))
        return;

    n = read (kbdFd, &ev, sizeof (ev));

    if (n < 0)
    {
        mprintf (LVL_KBD, "problem reading from device %s: %s\n",
                kbdDevice, strerror (errno));
        kbdReopen ();
        return;
    }

    if (n < (int) sizeof (ev))
    {
        mprintf (LVL_KBD, "Partial read of HID device, got %d / %lu bytes ", n,
          sizeof (struct input_event));
        return;
    }

    decodeEvent (ev);
}

int kbdGet (int row, int col)
{
    if (keyState[row][col] != lastState[row][col])
    {
        mprintf (LVL_KBD, "%s scan row/col %d/%d = %d\n", __func__, row, col, keyState[row][col]);
        lastState[row][col] = keyState[row][col];
    }

    return keyState[row][col];
}

void kbdClose (void)
{
    if (kbdFd != -1)
    {
        mprintf (LVL_KBD, "%s closing\n", __func__);
        close (kbdFd);
        kbdFd = -1;
    }
}

void kbdOpen (const char *device)
{
    if (device)
        strcpy (kbdDevice, device);
    else
        kbdFindInputDevice ();

    kbdFd = -1;
    kbdReopen ();

    mprintf (LVL_KBD, "%s dev %s opened as fd %d\n", __func__, kbdDevice, kbdFd);

}

#ifdef __UNIT_TEST

int main (int argc, char *argv[])
{
    const char *device = "/dev/input/event7";
    
    outputLevel = LVL_KBD;

    if (argc > 1)
        device = argv[1];

    kbdOpen (device);

    while (1)
    {
        kbdPoll ();
        usleep (10000);
    }

    kbdClose ();
    return 0;
}

#endif

