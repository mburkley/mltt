/*
 *  Implements the console specifics, I/O, etc, of the 99-4a
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
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

typedef union
{
    BYTE b[0x2000];
}
memPage;

typedef struct
{
    BOOL    rom;
    BOOL    mapped;
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

int ti994aQuitFlag;
int ti994aRunFlag;

static void sigHandler (int signo)
{
    int i;
    int n;
    char **str;

    void *bt[10];

    switch (signo)
    {
    case SIGQUIT:
    case SIGTERM:
        printf ("quit seen\n");
        ti994aQuitFlag = 1;
        break;

    /*  If CTRL-C then stop running and return to console */
    case SIGINT:
        printf("Set run flag to zero\n");
        ti994aRunFlag = 0;
        break;

    case SIGSEGV:
        n = backtrace (bt, 10);
        str = backtrace_symbols (bt, n);

        if (str)
        {
            for (i = 0; i < n; i++)
            {
                printf ("%s\n", str[i]);
            }
        }
        else
            printf ("failed to get a backtrace\n");

        exit (1);
        break;

    default:
        printf ("uknown sig %d\n", signo);
        exit (1);
        break;
    }
}

static void ti994aPrintScratchMemory (WORD addr, int len)
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
        WORD addr = 0x8300;

        while (addr >= 0x8300 && addr < 0x8400)
        {
            WORD nextAddr = gplScratchPadNext (addr);
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

WORD memRead(WORD addr, int size)
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

void memWrite(WORD addr, WORD data, int size)
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

WORD memReadB(WORD addr)
{
    return memRead (addr, 1);
}

void memWriteB(WORD addr, BYTE data)
{
    return memWrite (addr, data, 1);
}

WORD memReadW(WORD addr)
{
    return memRead (addr, 2);
}

void memWriteW(WORD addr, WORD data)
{
    return memWrite (addr, data, 2);
}

void ti994aMemLoad (char *file, WORD addr, WORD length)
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

        if (fread (p->m->b, sizeof (BYTE), 0x2000, fp) != 0x2000)
        {
            halt ("ROM file read failure");
        }
    }

    fclose (fp);
}

void ti994aKeyboardScan (int index, BYTE state)
{
    int row;
    int col = (cruBitGet(0, 20)<<2)|(cruBitGet(0, 19)<<1)|cruBitGet(0, 18);
    // int col = (cruBitGet(0, 18)<<2)|(cruBitGet(0, 19)<<1)|cruBitGet(0, 20);

    mprintf (LVL_CONSOLE, "KBD scan col %d (%d,%d,%d)\n", col, cruBitGet(0,18),
    cruBitGet(0,19),cruBitGet(0,20));
    for (row = 0; row < KBD_COL; row++)
    {
        int bit = kbdGet (row, col) ? 0 : 1;

         if (!bit)
             mprintf (LVL_CONSOLE, "KBD col %d, row %d active\n", col, row);

        cruBitInput (0, 3+row, bit);
    }
}

void ti994aInterrupt (int index, BYTE state)
{
    /*  Generate an interrupt for the each cru bit
     *  that is set.
     */
    int i;
    for (i = 1; i < 16; i++)
        if (!cruBitGet (0, i))
        {
            // printf("interrupt %d\n", i);
            /*  Interrupts to the TMS9900 are hardwired to always generate
             *  interrupt level 1 regardless of what device generated the
             *  interrupt */
            // TODO - interrupts are level triggered, so we should raise and
            // lower using an OR of all CRU inputs
            interruptRequest (1);
        }
}

void ti994aVideoInterrupt (void)
{
    static int count;
    vdpRefresh(0);
    soundUpdate();

    /*
     *  Clear bit 2 to indicate VDP interrupt
     */

    if (cruBitGet (0, 2))
    {
            // printf("video interrupt\n");
        cruBitSet (0, 2, 0);
    }

    if (++count == 50)
    {
        printf (".");
        // cpuShowStatus();
        count = 0;
    }
}

void ti994aRun (void)
{
    static int count;

    ti994aRunFlag = 1;

    printf("enter run loop\n");
    while (ti994aRunFlag &&
           !breakPointHit (cpuGetPC()) &&
           !conditionTrue ())
    {
        // tms9900.ram.covered[tms9900.pc>>1] = 1;

// gromIntegrity();
        kbdPoll ();
        timerPoll ();
        cpuExecute (cpuFetch());

        /*  To approximate execution speed at around 10 clock cycles per
         *  instruction with a 3MHz clock, we sleep for 3 usec for instruction
         */
        if (count++ == 100)
        {
        usleep (333);
        count = 0;
        }
// gromIntegrity();


        watchShow();
        // showGromStatus();
    }
}

void ti994aInit (void)
{
    if (signal(SIGSEGV, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register SEGV handler\n");
        exit (1);
    }

    if (signal(SIGQUIT, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register QUIT handler\n");
        exit (1);
    }

    if (signal(SIGTERM, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register TERM handler\n");
        exit (1);
    }

    if (signal(SIGINT, sigHandler) == SIG_ERR)
    {
        printf ("Failed to register INT handler\n");
        exit (1);
    }

    /*  Start a 20-msec (50Hz) recurring timer to generate video interrupts */
    timerStart (20, ti994aVideoInterrupt);

    printf ("** set initial CRU states\n");
    cruBitSet (0, 0, 0); // control
    cruBitSet (0, 1, 1); // externl irq
    cruBitSet (0, 2, 1); // /VDP irq
    cruBitInput (0, 3, 1); // kbd = line
    cruBitInput (0, 4, 1); // kbd space line
    cruBitInput (0, 5, 1); // kbd enter line
    cruBitInput (0, 6, 1); // kbd O line
    cruBitInput (0, 7, 1); // kbd FCTN line
    cruBitInput (0, 8, 1); // kbd SHIFT line
    cruBitInput (0, 9, 1); // kbd CTRL line
    cruBitInput (0, 10, 1); // kbd Z line
    cruBitInput (0, 11, 1); // not used
    cruBitInput (0, 12, 1); // reserved
    cruBitInput (0, 13, 1); // not used
    cruBitInput (0, 14, 1); // not used
    cruBitInput (0, 15, 1); // not used
    cruBitInput (0, 16, 0); // reserved
    cruBitInput (0, 17, 0); // reserved
    cruBitSet (0, 18, 0); // bit 2 kbd sel
    cruBitSet (0, 19, 0); // bit 1 kbd sel
    cruBitSet (0, 20, 0); // bit 0 kbd sel
    cruBitSet (0, 21, 0); // alpha lock column select
    cruBitSet (0, 22, 0); // cassette motor 1
    cruBitSet (0, 23, 0); // cassette motor 2
    cruBitSet (0, 24, 0); // audio gate
    cruBitSet (0, 25, 0); // tape output
    cruBitInput (0, 26, 0); // reserved
    cruBitInput (0, 27, 0); // tape input
    cruBitSet (0, 28, 0); // not used
    cruBitSet (0, 29, 0); // not used
    cruBitSet (0, 30, 0); // not used
    cruBitSet (0, 31, 0); // not used
    int i;

    /*  Attach handlers to CRU bits that generate interrupts */
    for (i = 1; i <= 2; i++)
        cruCallbackSet (i, ti994aInterrupt);

    /*  Attach handlers to CRU bits that are for keyboard input */
    for (i = 20; i <= 20; i++)
    // for (i = 18; i <= 21; i++)
        cruCallbackSet (i, ti994aKeyboardScan);
}

void ti994aClose (void)
{
    timerStop ();
}

