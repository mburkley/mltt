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

