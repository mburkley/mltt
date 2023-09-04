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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "trace.h"

static int timerFd;
static void (*timerCallback) (void);

void timerStart (int msec, void (*callback)(void))
{
    struct itimerspec newval;
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        halt("clock_gettime");

    newval.it_value.tv_sec = 0;
    newval.it_value.tv_nsec = msec * 1000000;
    newval.it_interval.tv_sec = 0;
    newval.it_interval.tv_nsec = msec * 1000000;
    timerFd = timerfd_create (CLOCK_MONOTONIC, 0);
    if (timerFd == -1)
        halt("timerfd_create");

    if (timerfd_settime(timerFd, /* TFD_TIMER_ABSTIME */ 0, &newval, NULL) == -1)
        halt("timerfd_settime");

    timerCallback = callback;
}

void timerStop (void)
{
    close (timerFd);
}

void timerPoll (void)
{
    int n;
    #if 0
    struct pollfd pfds[1];
    int ret;

    pfds[0].fd = timerFd;
    pfds[0].events = POLLIN;
    ret = poll(pfds, 1, 0);

    if (ret < 1)
    {
        return;
    }

    #endif
    u_int64_t data;
    n = read (timerFd, &data, sizeof (data));

    if (n != sizeof(data))
        halt("read");

    if (timerCallback)
        timerCallback ();
}

