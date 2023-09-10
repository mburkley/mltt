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
#include "timer.h"

#define NSEC_PER_SEC            1000000000 // 1 billion nanosecs in a second

struct _timers
{
    int fd;
    int nsec;
    void (*callback) (void);
    bool running;
}
timers[MAX_TIMERS];

static void timespecSet (struct timespec *ts, int nsec)
{
    ts->tv_sec = nsec / NSEC_PER_SEC;
    ts->tv_nsec = (nsec % NSEC_PER_SEC);
}

static void timerReset (int index, int nsec)
{
    struct itimerspec startval;

    timespecSet (&startval.it_value, nsec);
    timespecSet (&startval.it_interval, nsec);

    // printf("[t%d timerfd reset to %d]\n", index, nsec);
    if (timerfd_settime(timers[index].fd, /* TFD_TIMER_ABSTIME */ 0, &startval, NULL) == -1)
    {
        perror ("timer");
        halt("timerfd_settime");
    }
}

/*  Start a count down timer.  If a timer is already running, it is reset.
 *  Passing a value of 0 for microsends stops the timer.
 */
void timerStart (int index, int nsec, void (*callback)(void))
{
    if (index < 0 || index >= MAX_TIMERS)
        halt ("bad timer index");

    timers[index].callback = callback;
    timers[index].nsec = nsec;

    timers[index].running = (nsec>0) ? true : false;

    timerReset (index, nsec);
}

void timerStop (int index)
{
    // printf ("stop-%d\n", index);
    timerReset (index, 0);
}

/*  Do a blocking read on the timers.  This synchronises the system at 50Hz or a
 *  lower value if specified.
 *  This function used to poll and the main function used to sleep but it's
 *  easier just to do a blocking read
 */
void timerPoll (void)
{
    struct pollfd pfds[MAX_TIMERS];
    int i;

    for (i = 0; i < MAX_TIMERS; i++)
    {
        pfds[i].fd = timers[i].fd;
        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
    }

    int expired;

    // printf("[p]\n");
    expired = poll(pfds, MAX_TIMERS, -1);

    if (expired == 0)
        return;

    if (expired < 0)
    {
        printf ("Problem polling timers\n");
        perror ("poll");
        return;
    }

    // printf("poll-expired\n");
    for (i = 0; i < MAX_TIMERS; i++)
    {
        if ((pfds[i].revents & POLLIN) != 0)
        {
            // printf ("[exp:%d]\n", i);
            u_int64_t data;
            int n = read (timers[i].fd, &data, sizeof (data));

            if (n != sizeof(data))
                halt("read");

            break;
        }
    }
    if (i == MAX_TIMERS)
        halt ("timer expired, but none are pollable");

    if (timers[i].callback)
        timers[i].callback ();
}

void timerInit (void)
{
    for (int i = 0; i < MAX_TIMERS; i++)
    {
        timers[i].fd = timerfd_create (CLOCK_MONOTONIC, 0);

        if (timers[i].fd == -1)
            halt("timerfd_create");
    }
}

void timerClose (void)
{
    for (int i = 0; i < MAX_TIMERS; i++)
    {
        if (timers[i].fd != -1)
        {
            close (timers[i].fd);
            timers[i].fd = -1;
        }
    }
}

