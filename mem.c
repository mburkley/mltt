#ifdef __UNIT_TEST

#include <stdarg.h>

void halt (char *s)
{
    s=s;
}

int mprintf (int level, char *s, ...)
{
    va_list ap;

    level = level;
    va_start (ap, s);

    vprintf (s, ap);

    va_end (ap);

    return 0;
}

int main (void)
{
    WORD *r0, *r8;

    r0 = memWordAddr (0x83E0);
    r8 = memWordAddr (0x83F0);

    printf ("Set R0 to AA55 and R8 to A55A\n");
    *r0 = 0xAA55;
    *r8 = 0xA55A;

    printf ("memRead(83E0)  = %04X\n", memRead (0x83E0));
    printf ("memReadB(83E0) = %04X\n", memReadB (0x83E0));
    printf ("memReadB(83E1) = %04X\n", memReadB (0x83E1));

    printf ("memRead(83F0)  = %04X\n", memRead (0x83F0));
    printf ("memReadB(83F0) = %04X\n", memReadB (0x83F0));
    printf ("memReadB(83F1) = %04X\n", memReadB (0x83F1));

    return 0;
}

#endif

