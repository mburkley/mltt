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

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static int kbdFd;
static const char *kbdDevice;

/*
 *  *=unused
 *  F=function
 *  S=shift
 *  C=control
 */
static char keyMap[KBD_ROW][KBD_COL] =
{
    { '=',  '.', ',', 'm', 'n', '/', 0, 0 }, // JS fire
    { ' ',  'l', 'k', 'j', 'h', ';', 0, 0 }, // JS left
    { '\n', 'o', 'i', 'u', 'y', 'p', 0, 0 }, // JS right
    { '*',  '9', '8', '7', '6', '0', 0, 0 }, // JS down
    { 'F',  '2', '3', '4', '5', '1', 0, 0 }, // JS up / alpha lock
    { 'S',  's', 'd', 'f', 'g', 'a', 0, 0 },
    { 'C',  'w', 'e', 'r', 't', 'q', 0, 0 },
    { '*',  'x', 'c', 'v', 'b', 'z', 0, 0 }
};

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

#if 0
static struct _
{
}
keyExtended[] =
{
    { 14, ?? }, // Backspace => FNCT+???
    { 105,      // Left = FNCT+S
    { 106       // Right = FNCT+D
    { 103       // Up = FNCT+E
    { 108       // Down = FNCT+X
    { 58 // caps lock
    { 54 // right shift
    { 97 // right control
    { 100 // altgr = alphalock?
    
    101010101010101010101010

#endif
static int keyState[KBD_ROW][KBD_COL];

#if 0
static int digitMap[] =
{
    82, // KEY_KP0
    79, // KEY_KP1
    80, // KEY_KP2
    81, // KEY_KP3
    75, // KEY_KP4
    76, // KEY_KP5
    77, // KEY_KP6
    71, // KEY_KP7
    72, // KEY_KP8
    73, // KEY_KP9
};
#endif

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
                    mprintf (LVL_KBD, "%s KEY %s -> '%c' (%d,%d)\n",
                             __func__,
                             ev.value == 0 ? "UP" : "DOWN",
                             keyMap[i][j], i, j);
                    keyState[i][j] = ev.value;
                    mapped = 1;
                }

        if (!mapped)
            mprintf (LVL_KBD, "%s unkonwn KEY %s not mapped c=%d\n", __func__,
                   ev.value == 0 ? "UP" : "DOWN", ev.code);

    }
}

void kbdPoll (void)
{
    struct input_event ev;
    struct pollfd pfds[2];
    // struct pollfd pfds[2];
    int n;
    int ret;
    // char val[2];

    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[1].fd = kbdFd;
    pfds[1].events = POLLIN;
    ret = poll(pfds, 2, 0);

    if (ret < 1)
        return;

    #if 0
    if (pfds[0].revents & POLLIN)
    {
        /*  If key pressed, read and discard a char from stdin*/
        printf ("reading a key...\n");
        fgetc(stdin);
    }
    #endif

    if (!(pfds[1].revents & POLLIN))
        return;

    // mprintf(LVL_KBD, "KBD Data available\n");
    // printf ("Read from fd %d\n", fd);
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
    static int lastState[8][8];    // if (keyState[row][col])

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

    /* restore terminal attributes */
    // tcsetattr(0, TCSANOW, &oldtio);
}

void kbdOpen (const char *device)
{
    kbdDevice = device;
    kbdFd = -1;
    kbdReopen ();
    #if 0
    struct termios tio;
    tcgetattr(0, &tio);
    tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &tio);

    #endif
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

