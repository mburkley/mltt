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
 *  Implements the console specifics, I/O, etc, of the 99-4a
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "cpu.h"
#include "mem.h"
#include "sound.h"
#include "speech.h"
#include "grom.h"
#include "vdp.h"
#include "trace.h"
#include "break.h"
#include "watch.h"
#include "cond.h"
#include "unasm.h"
#include "cover.h"
#include "kbd.h"
#include "cru.h"
#include "timer.h"
#include "interrupt.h"
#include "gpl.h"
#include "cassette.h"
#include "ti994a.h"

typedef union
{
    uint8_t b[0x2000];
}
memPage;

typedef struct
{
    bool    rom;
    bool    mapped;
    int addr;
    int mask;
    memPage *m;
    int (*readHandler)(int addr, int size);
    void (*writeHandler)(int addr, int size, int value);
}
memMap;

memPage ram[4];
memPage rom[3];
memPage scratch;

memMap memory[] =
{
    { 1, 0, 0x0000, 0x1FFF, &rom[0], NULL, NULL }, // Console ROM
    { 0, 0, 0x2000, 0x1FFF, &ram[0], NULL, NULL }, // 32k Expn low
    { 1, 0, 0x4000, 0x1FFF, &rom[1], NULL, NULL }, // Device ROM (selected by CRU)
    { 1, 0, 0x6000, 0x1FFF, &rom[2], NULL, NULL }, // Cartridge ROM
    { 0, 1, 0x8000, 0x00FF, &scratch, NULL, NULL }, // MMIO + scratchpad
    { 0, 0, 0xA000, 0x1FFF, NULL,    NULL, NULL }, //
    { 0, 0, 0xC000, 0x1FFF, &ram[2], NULL, NULL }, // + 32k expn high
    { 0, 0, 0xE000, 0x1FFF, &ram[3], NULL, NULL }  //
};

memMap mmio[] =
{
    { 0, 0, 0x8000, 0x00FF, &scratch, NULL, NULL }, // Scratch pad RAM
    { 0, 1, 0x8400, 0x00FF, NULL   , soundRead, soundWrite }, // Sound device
    { 0, 1, 0x8800, 0x0003, NULL   , vdpRead, NULL }, // VDP Read
    { 0, 1, 0x8C00, 0x0003, NULL   , NULL, vdpWrite }, // VDP Write
    { 0, 1, 0x9000, 0x0003, NULL   , speechRead, NULL }, // Speech Read
    { 0, 1, 0x9400, 0x0003, NULL   , NULL, speechWrite }, // Speech Write
    { 0, 1, 0x9800, 0x0003, NULL   , gromRead, NULL }, // GROM Read
    { 0, 1, 0x9C00, 0x0003, NULL   , NULL, gromWrite }, // GROM Write
};

bool ti994aRunFlag;

static void ti994aPrintScratchMemory (uint16_t addr, int len)
{
    int i, j;

    for (i = addr; i < addr+len; i += 16 )
    {
        printf ("\n%04X - ", i + 0x8300);

        for (j = i; j < i + 16 && j < addr+len; j += 2)
        {
            if (len == 1)
                printf ("%02X   ", scratch.b[j+1]);
            else
                printf ("%02X%02X ", scratch.b[j], scratch.b[j+1]);
        }

        for (; j < i + 16; j += 2)
            printf ("     ");
    }
}

/*  Show the contents of the scratchpad memory either in abbreviated or detailed
 *  form.  If the param is false, just a hex dump is shown.  If true, each value
 *  is presented on a separate line with a description
 */
void ti994aShowScratchPad (bool showGplUsage)
{
    printf ("Scratchpad\n");
    printf ("==========");

    if (showGplUsage)
    {
        uint16_t addr = 0x8300;

        while (addr >= 0x8300 && addr < 0x8400)
        {
            uint16_t nextAddr = gplScratchPadNext (addr);
            ti994aPrintScratchMemory (addr - 0x8300, nextAddr - addr);
            gplShowScratchPad(addr);
            addr = nextAddr;
        }
    }
    else
        ti994aPrintScratchMemory (0, 0x100);

    printf ("\n");
}

static memMap *memMapEntry (int addr)
{
    if ((addr & 0xE000) == 0x8000)
        return &mmio[(addr & 0x1FFF) >> 10];

    return &memory[addr>>13];
}

uint16_t memRead(uint16_t addr, int size)
{
    memMap *p = memMapEntry (addr);

    if (p->mapped)
    {
        if (p->readHandler)
            return p->readHandler (addr & p->mask, size);
        else
            goto error;
    }

    if (!p->m)
    {
        /*
         *  Not installed
         */
        goto error;
    }

    if (size == 2)
    {
        addr &= ~1; // Always round word reads to a word boundary?
        mprintf (LVL_CONSOLE, "[memReadW:%04X->%02X%02X]", addr,
        p->m->b[addr&p->mask], p->m->b[(addr&p->mask)+1]);

        return (p->m->b[addr&p->mask] << 8) | p->m->b[(addr&p->mask)+1];
    }
    else
    {
        mprintf (LVL_CONSOLE, "[memReadB:%04X->%02X]", addr,
        p->m->b[addr&p->mask]);

        return p->m->b[addr&p->mask];
    }

error:
    return 0xff;
}

void memWrite(uint16_t addr, uint16_t data, int size)
{
    memMap *p = memMapEntry (addr);

    if (p->mapped)
    {
        if (p->writeHandler)
            return p->writeHandler (addr & p->mask, data, size);
        else
        {
            printf("mapped\n");
            goto error;
        }
    }

    if (!p->m)
    {
        /*
         *  Not installed
         */
        printf ("no mem\n");
        goto error;
    }

    if (p->rom)
    {
        printf ("addr=%04X\n", addr);
        halt ("Attempted write to ROM");
    }

    if (size == 2)
    {
        addr &= ~1; // Always round word reads to a word boundary?

        p->m->b[addr&p->mask] = (data >> 8) & 0xFF;
        p->m->b[(addr&p->mask)+1] = data & 0xFF;
        mprintf (LVL_CONSOLE, "[memWriteW:%04X<-%02X%02X]", addr,
        p->m->b[addr&p->mask],
                p->m->b[(addr&p->mask)+1]);
    }
    else
    {
        p->m->b[addr&p->mask] = data;
        mprintf (LVL_CONSOLE, "[memWriteB:%04X<-%02X]", addr,
        p->m->b[addr&p->mask]);
    }
    return;

error:
    printf ("write to unmapped addr %x\n", addr);
    halt ("write");
}

uint16_t memReadB(uint16_t addr)
{
    return memRead (addr, 1);
}

void memWriteB(uint16_t addr, uint8_t data)
{
    return memWrite (addr, data, 1);
}

uint16_t memReadW(uint16_t addr)
{
    return memRead (addr, 2);
}

void memWriteW(uint16_t addr, uint16_t data)
{
    return memWrite (addr, data, 2);
}

void ti994aMemLoad (char *file, uint16_t addr, uint16_t length)
{
    FILE *fp;
    int i;
    memMap *p;

printf("%s %s %x %x\n", __func__, file, addr, length);
    if ((fp = fopen (file, "rb")) == NULL)
    {
        printf ("can't open ROM bin file '%s'\n", file);
        exit (1);
    }

    for (i = addr; i < addr + length; i += 0x2000)
    {
        p = &memory[i>>13];

        if (fread (p->m->b, sizeof (uint8_t), 0x2000, fp) != 0x2000)
        {
            halt ("ROM file read failure");
        }
    }

    fclose (fp);
}

void ti994aVideoInterrupt (void)
{
    vdpRefresh(0);
    // soundUpdate();

    /*
     *  Clear bit 2 to indicate VDP interrupt
     */
    mprintf (LVL_INTERRUPT, "IRQ_VDP lowered\n");
    cruBitInput (0, IRQ_VDP, 0);
}

void ti994aRun (int instPerInterrupt)
{
    static int count;

    ti994aRunFlag = true;
    printf("enter run loop\n");

    while (ti994aRunFlag &&
           !breakPointHit (cpuGetPC()) &&
           !conditionTrue ())
    {
        uint16_t opcode = cpuFetch ();
        bool shouldBlock = false;

        /*  Check if the instruction we are about to execute is >10FF, which is
         *  an infinite loop.  It is used to wait for an interrupt during
         *  cassette operations.  There is no need to actually spin, just go
         *  straight to a blocking read on the interrupt timer.
         */
        if (opcode == 0x10FF)
        {
            shouldBlock = true;
        }

        /*  To approximate execution speed at around 10 clock cycles per
         *  instruction with a 3MHz clock, we expect to execute about 2000
         *  instructions per VDP interrupt so do a blocking timer read once
         *  count reaches this value.
         *
         *  TODO : trick, LIMI 2 is the main GPL loop but won't mess up cassette
         *  ops.  Will this work with munchman and other ROM games?
         */
        if (cpuGetIntMask() == 2 && count >= instPerInterrupt)
        {
            shouldBlock = true;
            count -= instPerInterrupt;
        }

        if (shouldBlock)
        {
            kbdPoll ();
            timerPoll ();
        }

        cpuExecute (opcode);
        count++;

        watchShow();
    }

    ti994aRunFlag = false;
}

void ti994aInit (void)
{
    tms9901Init ();
    timerInit ();

    /*  Start a 20-msec (20,000,000 nanosec == 50Hz) recurring timer to generate video interrupts */
    timerStart (TIMER_VDP, 20000000, ti994aVideoInterrupt);

    int i;

    /*  Attach handler to bit 0 to set mode */
    cruOutputCallbackSet (0, tms9901ModeSet);

    /*  Attach handlers to CRU bits that generate interrupts (input), mask
     *  interrupts (output in interrupt mode) or set the clock (output in timer
     *  mode).
     */
    for (i = 1; i <= 15; i++)
    {
        cruBitInput (0, i, 1);  // Initial state inactive (active low)
        cruInputCallbackSet (i, tms9901Interrupt);
        cruOutputCallbackSet (i, tms9901BitSet);
        cruReadCallbackSet (i, tms9901BitGet);
    }

    /*  Attach handlers to CRU bits that are for keyboard input column select.
     */
    for (i = 18; i <= 20; i++)
        cruOutputCallbackSet (i, kbdColumnUpdate);

    cruOutputCallbackSet (22, cassetteMotor);
    cruOutputCallbackSet (23, cassetteMotor);
    cruOutputCallbackSet (24, cassetteAudioGate);
    cruOutputCallbackSet (25, cassetteTapeOutput);
    cruReadCallbackSet (27, cassetteTapeInput);
}

void ti994aClose (void)
{
    timerClose ();
}

