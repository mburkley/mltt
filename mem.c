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

/*
 *  Implements the memory map of the '4A
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "types.h"
#include "trace.h"
#include "mem.h"
#include "sound.h"
#include "grom.h"
#include "vdp.h"
#include "speech.h"
#include "fdd.h"
#include "sams.h"

typedef struct _memMap
{
    int addr;
    int mask;
    struct _memMap *submap;
    int bits;
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
static unsigned char *mmapRegion;
static int deviceSelected;

static uint16_t dataRead (uint8_t *data, uint16_t addr, int size);
static void dataWrite (uint8_t *data, uint16_t addr, uint16_t value, int size);
static uint16_t invalidRead (uint8_t *data, uint16_t addr, int size);
static void invalidWrite (uint8_t *data, uint16_t addr, uint16_t value, int size);
static void mmapWrite (uint8_t *data, uint16_t addr, uint16_t value, int size);
static void bankSelect (uint8_t *data, uint16_t addr, uint16_t value, int size);
uint16_t deviceRead (uint8_t *ptr, uint16_t addr, int size);
void deviceWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size);

/*  Memory mapped I/O region.  0x2000 in size with 0x400 byte pages */
memMap mapMmio[] =
{
    { 0x8000, 0x00FF, NULL, 0, scratch, dataRead, dataWrite }, // Scratch pad RAM
    { 0x8400, 0x00FF, NULL, 0, NULL   , soundRead, soundWrite }, // Sound device
    { 0x8800, 0x0003, NULL, 0, NULL   , vdpRead, invalidWrite }, // VDP Read
    { 0x8C00, 0x0003, NULL, 0, NULL   , invalidRead, vdpWrite }, // VDP Write
    { 0x9000, 0x0003, NULL, 0, NULL   , speechRead, invalidWrite }, // Speech Read
    { 0x9400, 0x0003, NULL, 0, NULL   , speechRead, speechWrite }, // Speech Write
    { 0x9800, 0x0003, NULL, 0, NULL   , gromRead, invalidWrite }, // GROM Read
    { 0x9C00, 0x0003, NULL, 0, NULL   , invalidRead, gromWrite }, // GROM Write
};

/*  Cartridge area.  Split into two 4k blocks for minimem */
memMap mapCart[] =
{
    { 0x6000, 0x0FFF, NULL, 0, romCartridge[0], dataRead, bankSelect }, // Cartridge ROM
    { 0x7000, 0x0FFF, NULL, 0, &romCartridge[0][0x1000], dataRead, mmapWrite }, // Cartridge ROM
};

/*  Main memory map in 8k pages */
memMap mapMain[] =
{
    { 0x0000, 0x1FFF, NULL,     0, romConsole, dataRead, invalidWrite }, // Console ROM
    { 0x2000, 0x1FFF, NULL,     0, &ram[0x0000], dataRead, dataWrite }, // 32k Expn low
    { 0x4000, 0x1FFF, NULL,     0, romDevice[0], deviceRead, deviceWrite }, // Device ROM (selected by CRU)
    { 0x6000, 0x1FFF, mapCart, 12, NULL, NULL, NULL }, // Cartridge ROM
    { 0x8000, 0x1FFF, mapMmio, 10, NULL, NULL, NULL }, // MMIO + scratchpad
    { 0xA000, 0x1FFF, NULL,     0, &ram[0x2000], dataRead, dataWrite }, //
    { 0xC000, 0x1FFF, NULL,     0, &ram[0x4000], dataRead, dataWrite }, // + 32k expn high
    { 0xE000, 0x1FFF, NULL,     0, &ram[0x6000], dataRead, dataWrite }  //
};

/*  Read a device ROM.  Some devices have memory mapped I/O in their ROM address
 *  space.  TODO add a process for devices to register this MMIO
 */
uint16_t deviceRead (uint8_t *ptr, uint16_t addr, int size)
{
    if (deviceSelected == 1 && (addr&0x1FF0)==0x1FF0)
        return fddRead (addr&0xF, size);

    return dataRead (ptr, addr, size);
}

void deviceWrite (uint8_t *ptr, uint16_t addr, uint16_t data, int size)
{
    if (deviceSelected == 1 && (addr&0x1FF0)==0x1FF0)
        fddWrite (addr&0xF, data, size);
    else if (deviceSelected == 14) // SAMS card - paging TODO
        dataWrite (ptr, addr, data, size);
    else
        invalidWrite (ptr, addr, data, size);
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

static void mmapWrite (uint8_t *data, uint16_t addr, uint16_t value, int size)
{
    if (!mmapRegion)
    {
        printf ("Invalid Write unmapped %04X to %04X\n", value, addr);
        halt ("invalid write");
    }

    dataWrite (mmapRegion, addr, value, size);
}

/*  Select ROM in a cartridge with multiple ROMs.  This is crude for now and
 *  just follows the EB scheme where a write to address >6002 selects the second
 *  ROM.  Addr is relative to 0x6000
 */
static void bankSelect (uint8_t *data, uint16_t addr, uint16_t value, int size)
{
    mprintf (LVL_CONSOLE, "Bank Write %04X to %04X\n", value, addr);

    /*  Extended basic writes to addresses 0x0000 and 0x0002.  Pacman writes to
     *  0x001C and 0x001E.  Can't find a standard way to select cartridge ROMs
     *  so just checking the least significant 2 bits to cover these two caes.
     */
    if ((addr & 3) == 2)
        mapMain[3].data = romCartridge[1];
    else
        mapMain[3].data = romCartridge[0];
}

bool memDeviceRomSelect (int index, uint8_t state)
{
    /*  Devices are selected through CRU bits >1000 thru >1F00.  Divided by 2 is
     *  >800 thru >F80.  We convert this to 0-15
     */
    deviceSelected = (index & 0x780) >> 7;

    mprintf (LVL_CONSOLE, "Select device ROM %d state %d\n", deviceSelected, state);

    /*  For now, assume device 0 means no device */
    if (state == 0)
        mapMain[2].data = romDevice[0];
    else
        mapMain[2].data = romDevice[deviceSelected];

    /*  Allow the bit state to be changed */
    return false;
}

static memMap *memMapEntry (int addr)
{
    memMap *m = &mapMain[addr>>13];

    while (1)
    {
        if (!m->submap)
            break;

        addr &= m->mask;
        m = &m->submap[addr>>m->bits];
    }

    return m;
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

int memLoad (char *file, uint16_t addr, int bank)
{
    FILE *fp;

    if ((fp = fopen (file, "rb")) == NULL)
    {
        printf ("can't open ROM bin file '%s'\n", file);
        halt ("load");
    }

    fseek (fp, 0, SEEK_END);
    int len = ftell (fp);
    fseek (fp, 0, SEEK_SET);

    printf("%s %s %x %x %d\n", __func__, file, addr, len, bank);

    #if 0
    if (len > ROM_FILE_SIZE)
    {
        printf ("ROM file size unsupported %04X\n", len);
        halt ("ROM file size");
    }
    #else
    if (len > ROM_FILE_SIZE)
        len = ROM_FILE_SIZE;
    #endif

    memMap *map = memMapEntry (addr);
    uint8_t *data = map->data;

    if (addr == 0x6000 && bank == 1)
        data = romCartridge[bank];

    if (addr == 0x4000)
        data = romDevice[bank];

    int got;
    if ((got=fread (data, sizeof (uint8_t), len, fp)) != len)
    {
        printf ("%s expected len=%04X, got=%04X\n", file, len, got);
        perror("fread");
        halt ("ROM file read failure");
    }

    fclose (fp);
    return len;
}

/*  Mmap a file into the region 0x7000 to 0x7FFF to emulate minimemory */
void memMapFile (const char *name, uint16_t addr, uint16_t size)
{
    if (addr != 0x7000 || size != 0x1000)
    {
        printf ("addr=%04X\n", addr);
        halt ("unsupported mmap address");
    }

    if (mmapRegion)
        halt ("mmap already mapped");

    int fd = open (name, O_RDWR);

    if (fd < 0)
    {
        printf ("file %s\n", name);
        halt ("open failure");
    }

    mmapRegion = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (!mmapRegion)
    {
        printf ("mmap failed to map %s len %d\n", name, size);
        halt ("mmap failure");
    }

    close (fd);
    mapCart[1].data = mmapRegion;
    printf ("%s mapped\n", name);
}

void memCopy (uint8_t *copy, uint16_t addr, int bank)
{
    memMap *map = &mapMain[addr>>13];
    uint8_t *data = map->data;

    if (addr == 0x6000 && bank == 1)
        data = romCartridge[bank];

    if (addr == 0x4000)
        data = romDevice[bank];

    memcpy (data, copy, ROM_FILE_SIZE);
}

/*  TODO move to console.c */
void memPrintScratchMemory (uint16_t addr, int len)
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

