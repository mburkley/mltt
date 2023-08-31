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

    #if 0
    static long execCount = 0;

        if (execCount++ >= 30000)
        {
            execCount = 0;

            // printf ("Refresh....\n");
            vdpRefresh(0);

            /*
             *  Clear and then reset bit 2 to indicate VDP interrupt
             */

            if (cruBitGet (0, 2))
            {
                cruBitSet (0, 2, 0);
                cruBitSet (0, 2, 1);
            }
        }
    #endif


void timerStart (int msec, void (*callback)(void))
{
    // struct timespec ts = { 0, 20000000 }; // 20 msec / 50 Hz
    struct itimerspec newval;
    // int max_exp;
    struct timespec now;
    // u_int64_t exp; // , tot_exp;
    // ssize_t s;

    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        halt("clock_gettime");

    // newval.it_value.tv_sec = now.tv_sec;
    // newval.it_value.tv_nsec = now.tv_nsec + msec * 1000000;
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
    struct pollfd pfds[1];
    int n;
    int ret;

    pfds[0].fd = timerFd;
    pfds[0].events = POLLIN;
    ret = poll(pfds, 1, 0);

    if (ret < 1)
    {
        // printf ("no poll events\n");
        return;
    }

    u_int64_t data;
    n = read (timerFd, &data, sizeof (data));

    if (n != sizeof(data))
        halt("read");

    // printf ("%s read %lu\n", __func__, data);

    if (timerCallback)
        timerCallback ();
}

