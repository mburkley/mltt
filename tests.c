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

/*
 *  Some basic CPU unit tests to check operations especially if behaviour is
 *  unclear
 */

#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "mem.h"
#include "sound.h"
#include "grom.h"
#include "vdp.h"
#include "trace.h"
#include "break.h"
#include "watch.h"
#include "cond.h"
#include "unasm.h"
#include "cover.h"
#include "cru.h"

static unsigned char testmem[0x200] =
{
    0x01, 0x00, // wp=0x100
    0x00, 0x04, // pc=4
    0x02, 0x07, // LI 7, 0xAAAA
    0xaa, 0xaa,

    // Test shift right with carry
    0x08, 0x27, // SRA 7, 2
    0x18, 0x02, // JOC +2
    0x07, 0x01, // SETO 1
    0x10, 0x01, // JMP +1
    0x04, 0xc1, // CLR 1

    // Test shift right with carry
    0x09, 0x17, // SRL 7, 1
    0x18, 0x02, // JOC +2
    0x07, 0x01, // SETO 1
    0x10, 0x01, // JMP +1
    0x04, 0xc1, // CLR 1

    // Test byte move
    0xc1, 0x47, // MOV 7,5
    0x04, 0xc1, // CLR 1
    0xD1, 0x41, // MOVB 1,5

    // Test CRU
    0x02, 0x01, // LI 1, 0xAA00
    0xAA, 0x00,
    0x30, 0xC1, // LDCR 1,3
    0x07, 0x01, // SETO 1
    0x35, 0xC1, // STCR 1,7

    // Test MOV comp to zero
    0x07, 0x01, // SETO 1
    0xC0, 0x41, // MOV 1,1
    0x16, 0x01, // JNE +1
    0x04, 0xc1, // CLR 1
    0x07, 0x01, // SETO 1

    // Test DEC with inverted JOC logic
    0x02, 0x01, // LI 1, 0x0002
    0x00, 0x02,
    0x06, 0x01, // DEC 1
    0x18, 0x01, // JOC +1
    0x04, 0xc1, // CLR 1
    0x07, 0x01, // SETO 1

    // Test byte fetch and store
    0x02, 0x01, // LI 1, 0x0006
    0x00, 0x06,
    0x07, 0x02, // SETO 2
    0xD0, 0x91, // MOVB *1,2

    0x00, 0x00
};

WORD memReadB(WORD addr)
{
    printf("# [%s %x=%x]\n", __func__, addr, testmem[addr]);
    return testmem[addr];
}

void memWriteB(WORD addr, BYTE data)
{
    testmem[addr] = data;
    printf("# [%s %x=%x]\n", __func__, addr, data);
}

WORD memReadW(WORD addr)
{
    return testmem[addr] << 8 | testmem[addr+1];
}

void memWriteW(WORD addr, WORD data)
{
    testmem[addr] = data >> 8;
    testmem[addr+1] = data & 0xff;
}

static int testsRun;

static void result (int a, int b)
{
    if (testsRun++ == 0)
        printf ("1..18\n");

    if (a==b)
        printf ("ok %d\n", testsRun);
    else
    {
        printf ("# %x != %x\n", a, b);
        printf ("not ok %d\n", testsRun);
    }
}
int main(void)
{
    // outputLevel = 63;
    // cruInit ();
    // gromLoad ();
        unasmRunTimeHookAdd();
     // vdpInitGraphics();
    printf ("# 1<<0 is %d\n", 1 << 0);
    result (sizeof (unsigned), 4);
    unsigned x=0xf000000;
    printf ("# 0xf000000>>8 is %x\n", x>>8);
    cpuBoot ();

    cpuExecute (cpuFetch());
    result (memReadW (0x10e), 0xAAAA);
    cpuExecute (cpuFetch());
    cpuShowStWord ();
    result (memReadW (0x10e), 0xEAAA);
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0);
    cpuExecute (cpuFetch());
    cpuShowStWord ();
    result (memReadW (0x10e), 0x7555);
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0xffff);
    cpuExecute (cpuFetch());
    result (memReadW (0x10a), 0x7555);
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    result (memReadW (0x10a), 0x0055);
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0xAA00);
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());

    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0x02FF);

    if (cruBitGet (0, 2))
        cruBitSet (0, 2, 0);

    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0xFFFF);
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0xFFFF);

    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0x0002);
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0x0001);
    cpuExecute (cpuFetch());
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0xFFFF);

    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0x0006);
    cpuExecute (cpuFetch());
    result (memReadW (0x104), 0xFFFF);
    cpuExecute (cpuFetch());
    result (memReadW (0x104), 0xAAFF);
    return 0;
}

