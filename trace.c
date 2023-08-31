#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "trace.h"
#include "vdp.h"

int outputLevel;

int mprintf (int level, char *s, ...)
{
    va_list ap;

    va_start (ap, s);

    if (level & outputLevel)
        vprintf (s, ap);

    va_end (ap);

    return 0;
}

void halt (char *s)
{
    // vdpRefresh(1);

    printf ("HALT: %s\n", s);

    exit (1);
}

