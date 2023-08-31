#include <stdio.h>
#include <stdlib.h>

#include "../unasm.c"
#include "../cpu.c"
#include "../trace.c"
#include "../cru.c"
#include "../cover.c"
#if 0
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
#endif


static unsigned char testmem[0x200] =
{
    0x01, 0x00, // wp=0x100
    0x00, 0x04, // pc=4
    0x02, 0x07, // LIMI 7, 0xAAAA
    0xaa, 0xaa,
    0x08, 0x27, // SRA 7, 2
    0x18, 0x02, // JOC +2
    0x07, 0x01, // SETO 1
    0x10, 0x01, // JMP +1
    0x04, 0xc1, // CRL 1
    0x09, 0x17, // SRL 7, 1
    0x18, 0x02, // JOC +2
    0x07, 0x01, // SETO 1
    0x10, 0x01, // JMP +1
    0x04, 0xc1, // CLR 1
    0xc1, 0x47, // MOV 7,5
    0x04, 0xc1, // CLR 1
    0xD1, 0x41, // MOVB 1,5
    0x02, 0x01, // LI 1, 0xAA00
    0xAA, 0x00,
    0x30, 0xC1, // LDCR 1,3
    0x07, 0x01, // SETO 1
    0x35, 0xC1  // STCR 1,7

};

WORD memReadB(WORD addr)
{
    printf("[%s %x=%x]", __func__, addr, testmem[addr]);
    return testmem[addr];
}

void memWriteB(WORD addr, BYTE data)
{
    testmem[addr] = data;
    printf("[%s %x=%x]", __func__, addr, data);
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

#if 0
static int cru[256];

void cruBitSet (WORD base, I8 bitOffset, BYTE state)
{
    printf ("cru set %x %x %x\n", base, bitOffset, state);
    cru[base/2+bitOffset]=state;
}

BYTE cruBitGet (WORD base, I8 bitOffset)
{
    printf ("cru get %x %x\n", base, bitOffset);
    return cru[base/2+bitOffset];
}
#endif

static void result (int a, int b)
{
    static int count;

    if (a==b)
        printf ("ok %d\n", ++count);
    else
    {
        printf ("not ok %d %x != %x\n", ++count, a, b);
        // exit(1);
    }
}

int main(void)
{
    outputLevel = 63;
    // cruInit ();
    // gromLoad ();
        unasmRunTimeHookAdd();
     // vdpInitGraphics();
    printf ("1..10\n");
    printf ("1<<0 is %d\n", 1 << 0);
    result (sizeof (unsigned), 4);
    unsigned x=0xf000000;
    printf ("0xf000000>>8 is %x\n", x>>8);
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
    printf("\nDo last\n");
    cpuExecute (cpuFetch());
    result (memReadW (0x102), 0x02FF);

    if (cruBitGet (0, 2))
        cruBitSet (0, 2, 0);

    return 0;
}


