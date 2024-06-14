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

#ifndef __TRACE_H
#define __TRACE_H

#define LVL_CONSOLE     0x0001
#define LVL_CPU         0x0002
#define LVL_VDP         0x0004
#define LVL_GROM        0x0008
#define LVL_UNASM       0x0010
#define LVL_CRU         0x0020
#define LVL_INTERRUPT   0x0040
#define LVL_KBD         0x0080
#define LVL_SOUND       0x0100
#define LVL_GPL         0x0200
#define LVL_GPLDBG      0x0400
#define LVL_CASSETTE    0x0800
#define LVL_DISK        0x1000

int mprintf (int level, const char *s, ...);
void halt (const char *s);
extern int outputLevel;
#define ASSERT(condition,text) \
{ \
    if (!(condition)) \
    { \
        printf ("assertion at %s:%d\n", __FILE__, __LINE__);\
        halt (text); \
    } \
}

#endif

