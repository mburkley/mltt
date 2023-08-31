#include "../trace.c"
#include "../timer.c"

static int cbcount;

void callback (void)
{
    printf ("%s\n", __func__);
    cbcount++;
}

int main (void)
{
    printf ("1..2\n");
    timerStart (20, callback);
    timerPoll ();
    usleep (30000);
    timerPoll ();
    if (cbcount == 1)
        printf ("ok 1 : 1 expiries\n");
    else
        printf ("not ok 1 : %d expiries\n", cbcount);
    usleep (10000);
    timerPoll ();
    if (cbcount == 2)
        printf ("ok 2 : 2 expiries\n");
    else
        printf ("not ok 2 : %d expiries\n", cbcount);
    timerStop();
    return 0;
}
