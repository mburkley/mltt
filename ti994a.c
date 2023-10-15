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
#include "disk.h"
#include "ti994a.h"

typedef struct
{
    bool    rom;
    bool    mapped;
    int addr;
    int mask;
    unsigned char *data;
    uint16_t (*readHandler)(uint8_t* ptr, uint16_t addr, int size);
    void (*writeHandler)(uint8_t* ptr, uint16_t addr, uint16_t value, int size);
}
memMap;

unsigned char ram[0x8000];
unsigned char romConsole[0x2000];
unsigned char romDevice[BANKS_DEVICE][0x2000];
unsigned char romCartridge[BANKS_CARTRIDGE][0x2000];
unsigned char scratch[0x100];

static int deviceSelected;

static uint16_t dataRead (uint8_t *data, uint16_t addr, int size);
static void dataWrite (uint8_t *data, uint16_t addr, uint16_t value, int size);
static uint16_t invalidRead (uint8_t *data, uint16_t addr, int size);
static void invalidWrite (uint8_t *data, uint16_t addr, uint16_t value, int size);
static void bankSelect (uint8_t *data, uint16_t addr, uint16_t value, int size);
uint16_t deviceRead (uint8_t *ptr, uint16_t addr, int size);
void deviceWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size);

memMap memory[] =
{
    { 1, 0, 0x0000, 0x1FFF, romConsole, dataRead, invalidWrite }, // Console ROM
    { 0, 0, 0x2000, 0x1FFF, &ram[0x0000], dataRead, dataWrite }, // 32k Expn low
    { 1, 0, 0x4000, 0x1FFF, romDevice[0], deviceRead, deviceWrite }, // Device ROM (selected by CRU)
    { 1, 0, 0x6000, 0x1FFF, romCartridge[0], dataRead, bankSelect }, // Cartridge ROM
    { 0, 1, 0x8000, 0x00FF, NULL, NULL, NULL }, // MMIO + scratchpad
    { 0, 0, 0xA000, 0x1FFF, &ram[0x2000], dataRead, dataWrite }, //
    { 0, 0, 0xC000, 0x1FFF, &ram[0x4000], dataRead, dataWrite }, // + 32k expn high
    { 0, 0, 0xE000, 0x1FFF, &ram[0x6000], dataRead, dataWrite }  //
};

memMap mmio[] =
{
    { 0, 0, 0x8000, 0x00FF, scratch, dataRead, dataWrite }, // Scratch pad RAM
    { 0, 1, 0x8400, 0x00FF, NULL   , soundRead, soundWrite }, // Sound device
    { 0, 1, 0x8800, 0x0003, NULL   , vdpRead, invalidWrite }, // VDP Read
    { 0, 1, 0x8C00, 0x0003, NULL   , invalidRead, vdpWrite }, // VDP Write
    { 0, 1, 0x9000, 0x0003, NULL   , speechRead, invalidWrite }, // Speech Read
    { 0, 1, 0x9400, 0x0003, NULL   , speechRead, speechWrite }, // Speech Write
    { 0, 1, 0x9800, 0x0003, NULL   , gromRead, invalidWrite }, // GROM Read
    { 0, 1, 0x9C00, 0x0003, NULL   , invalidRead, gromWrite }, // GROM Write
};

/*  Read a device ROM.  Some devices have memory mapped I/O in their ROM address
 *  space.  TODO add a process for devices to register this MMIO
 */
uint16_t deviceRead (uint8_t *ptr, uint16_t addr, int size)
{
    if (deviceSelected == 1 && (addr&0x1FF0)==0x1FF0)
        return diskRead (addr&0xF, size);

    return dataRead (ptr, addr, size);
}

void deviceWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size)
{
    if (deviceSelected == 1 && (addr&0x1FF0)==0x1FF0)
        return diskWrite (addr&0xF, data, size);

    return invalidWrite (ptr, addr, data, size);
}

static uint16_t dataRead (uint8_t *data, uint16_t addr, int size)
{
    if (size == 2)
    {
        addr &= ~1; // Always round word reads to a word boundary
        return (data[addr] << 8) | data[addr+1];
    }
    else
    {
        return data[addr];
    }
}

static void dataWrite (uint8_t *data, uint16_t addr, uint16_t value, int size)
{
    if (size == 2)
    {
        addr &= ~1; // Always round word reads to a word boundary
        data[addr++] = value >> 8;
        data[addr] = value & 0xFF;
    }
    else
    {
        data[addr] = value;
    }
}

static uint16_t invalidRead (uint8_t *data, uint16_t addr, int size)
{
    printf ("Read from %04X\n", addr);
    halt ("invalid read");
    return 0;
}

static void invalidWrite (uint8_t *data, uint16_t addr, uint16_t value, int size)
{
    printf ("Invalid Write %04X to %04X\n", value, addr);
    halt ("invalid write");
}

static void bankSelect (uint8_t *data, uint16_t addr, uint16_t value, int size)
{
    printf ("Bank Write %04X to %04X\n", value, addr);

    #if 1
    if (addr == 2)
        memory[3].data = romCartridge[1];
    else
        memory[3].data = romCartridge[0];
    #endif
}

bool ti994aDeviceRomSelect (int index, uint8_t state)
{
    /*  Devices are selected through CRU bits >1000 thru >1F00.  Divided by 2 is
     *  >800 thru >F80.  We convert this to 0-15
     */
    deviceSelected = (index & 0x780) >> 7;

    printf ("Select device ROM %d state %d\n", deviceSelected, state);

    /*  For now, assume device 0 means no device */
    if (state == 0)
        memory[2].data = romDevice[0];
    else
        memory[2].data = romDevice[deviceSelected];

    /*  Allow the bit state to be changed */
    return false;
}

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
                printf ("%02X   ", scratch[j+1]);
            else
                printf ("%02X%02X ", scratch[j], scratch[j+1]);
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
    return p->readHandler (p->data, addr & p->mask, size);
}

void memWrite(uint16_t addr, uint16_t data, int size)
{
    memMap *p = memMapEntry (addr);
    p->writeHandler (p->data, addr & p->mask, data, size);
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

void ti994aMemLoad (char *file, uint16_t addr, int bank)
{
    FILE *fp;

    if ((fp = fopen (file, "rb")) == NULL)
    {
        printf ("can't open ROM bin file '%s'\n", file);
        exit (1);
    }

    fseek (fp, 0, SEEK_END);
    int len = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    printf("%s %s %x %x %d\n", __func__, file, addr, len, bank);

    if (len != ROM_FILE_SIZE)
    {
        printf ("ROM file size unsupported %04X\n", len);
        halt ("ROM file size");
    }

    memMap *map = &memory[addr>>13];
    uint8_t *data = map->data;

    if (addr == 0x6000 && bank == 1)
        data = romCartridge[bank];

    if (addr == 0x4000)
        data = romDevice[bank];

    if (addr == 0x4000)
        data = romDevice[bank];

    if (fread (data, sizeof (uint8_t), ROM_FILE_SIZE, fp) != ROM_FILE_SIZE)
    {
        halt ("ROM file read failure");
    }

    fclose (fp);
}

void ti994aVideoInterrupt (void)
{
    vdpRefresh(0);

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
         *
         *  Messes up frogger, remove  TODO retest cassette
         */
        if (/* cpuGetIntMask() == 2 && */ count >= instPerInterrupt)
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

