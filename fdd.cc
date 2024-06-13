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
 *  Implements floppy disk controller.  A very minimal implementatoin, no errors
 *  or interrupts are ever generated.
 *
 *  Limitations:
 *
 *      * Fixed at 9 sectors per track and 40 tracks per disk
 *      * Fixed sector size of 256 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "types.h"
#include "mem.h"
#include "cpu.h"
#include "fdd.h"
#include "cru.h"
#include "interrupt.h"
#include "trace.h"

/*  Status bits are defined here but many are not used.  We are never busy,
 *  never not ready and we never have CRC errors or lost data.  We also never
 *  generate interrupts.
 */
#define DISK_STATUS_NOT_READY           0x80
#define DISK_STATUS_WRITE_PROTECT       0x40
#define DISK_STATUS_HEAD_ENGAGED        0x20
#define DISK_STATUS_WRITE_FAULT         0x20
#define DISK_STATUS_SEEK_ERROR          0x10
#define DISK_STATUS_CRC_ERROR           0x08
#define DISK_STATUS_LOST_DATA           0x04
#define DISK_STATUS_TRACK0              0x04
#define DISK_STATUS_INDEX               0x02
#define DISK_STATUS_DRQ                 0x02
#define DISK_STATUS_BUSY                0x01

static uint8_t diskId[6];
static uint8_t diskSector[DISK_BYTES_PER_SECTOR];
static int sectorsPerTrack = 9;
static int tracksPerSide = 40;

/*  Declare one extra drive handler as drives are numbered 1 to 3 and 0 means no
 *  drive selected */
static fddHandler driveHandler[DISK_DRIVE_COUNT+1];

static struct
{
    uint8_t command;
    uint8_t data;
    uint8_t sector;
    uint8_t track;
    uint8_t unit;
    uint8_t side;
    uint8_t *buffer;
    int bufferLen;
    int bufferPos;
    uint8_t status;
    bool direction; // true = inward
    bool ignoreIRQ;
    bool motorStrobe;
} fdd;

static void seekDisk (void)
{
    int sector = fdd.sector;

    if (fdd.side)
    {
        /*  Add the offset for the first side */
        sector += sectorsPerTrack * tracksPerSide;

        /*  For double sided disks saved in a sector-dump (.dsk file), the track
         *  number is "inverted" so track 39 is the first track on the second
         *  side and track 0 is the last track.
         */
        sector += (tracksPerSide - 1 - fdd.track) * sectorsPerTrack;
    }
    else
        sector += fdd.track * sectorsPerTrack;

    mprintf (LVL_DISK, "DSK - access sector %d [T:%d Sec:%d Side:%d]\n", sector, 
             fdd.track, fdd.sector, fdd.side);

    if (driveHandler[fdd.unit].seek)
        driveHandler[fdd.unit].seek (sector);
}

uint16_t fddRead (uint16_t addr, uint16_t size)
{
    uint8_t data;

    switch(addr)
    {
    case 0:
        mprintf (LVL_DISK, "DSK - read status=%02X\n", fdd.status);
        return ~fdd.status;
        break;
    case 2:
        mprintf (LVL_DISK, "DSK - read track=%02X\n", fdd.track);
        return ~fdd.track;
        break;
    case 4:
        mprintf (LVL_DISK, "DSK - read sector=%02X\n", fdd.sector);
        return ~fdd.sector;
        break;
    case 6:
        if (fdd.buffer)
            data = fdd.buffer[fdd.bufferPos++];
        else
            data = fdd.data;

        // if (fdd.bufferPos<6)
            mprintf (LVL_DISK, "DSK - read data [%02X]=%02X\n",
            fdd.bufferPos-1,data);

        if (fdd.bufferPos == fdd.bufferLen)
        {
            mprintf (LVL_DISK, "DSK - read finished\n");
            fdd.bufferPos = 0;
            fdd.buffer = NULL;
        }
        return ~data;
    default:
        mprintf (LVL_DISK, "DSK - unknown data\n");
        break;
    }
    return 0;
}

static void trackUpdate (bool inward)
{
    if (inward)
    {
        if (fdd.track < tracksPerSide)
            fdd.track++;
    }
    else
    {
        if (fdd.track > 0)
            fdd.track--;
    }
}

void fddWrite (uint16_t addr, uint16_t data, uint16_t size)
{
    /*  Data bus is inverted in FD1771 */
    data = (~data & 0xFF);

    fdd.status = 0;

    switch(addr)
    {
    case 0x8:
        mprintf (LVL_DISK, "DSK - cmd %02X : ", data);

        /*  We decode the command but we ignore many of the parameters.
         *  Verification and speed have no meaning here for example.
         */
        if (data < 0x80)
            mprintf (LVL_DISK, "speed %d, %s, %s, ", data&3, data&4?"verify":"no-verify",
                    data&8?"load" : "no-load");

        if (data >= 0x80 && data < 0xc0)
            mprintf (LVL_DISK, "%s, %s, ", data&8?"IBM":"non-IBM", data&4?"HLD":"no-delay");

        switch (data & 0xF0)
        {
        case 0x00:
            mprintf (LVL_DISK, "restore\n");
            fdd.track = 0;
            fdd.direction = true;
            fdd.status |= DISK_STATUS_TRACK0;
            break;
        case 0x10:
            /*  "[Seek] assumes that the Track Register contains the track
             *  number of the current position of the Read-Write head and the
             *  Data Register contains the desired track number"
             *
             *  So we just copy the data register to the track register to
             *  complete the seek.
             */
            fdd.track = fdd.data;
            mprintf (LVL_DISK, "seek T=%d, S=%d\n", fdd.track, fdd.sector);
            if (fdd.track == 0)
                fdd.status |= DISK_STATUS_TRACK0;
            break;
        case 0x20:
        case 0x30:
            /*  The update flag is meaningless since here all seeks are
             *  instantaneous.  The track register always reflects the desired
             *  track
             */
            mprintf (LVL_DISK, "step\n");
            trackUpdate (fdd.direction);
            break;
        case 0x40:
        case 0x50:
            mprintf (LVL_DISK, "step in\n");
            trackUpdate (true);
            break;
        case 0x60:
        case 0x70:
            mprintf (LVL_DISK, "step out\n");
            trackUpdate (false);
            break;
        case 0x80:
            mprintf (LVL_DISK, "read single sector\n");
            seekDisk();

            if (driveHandler[fdd.unit].read)
                driveHandler[fdd.unit].read (diskSector);

            fdd.buffer = diskSector;
            fdd.bufferPos = 0;
            fdd.bufferLen = DISK_BYTES_PER_SECTOR;
            break;
        case 0x90:
            printf ("TODO read multiple sector\n");
            break;
        case 0xA0:
            mprintf (LVL_DISK, "write single sector mark=%d\n", data&3);
            fdd.buffer = diskSector;
            fdd.bufferPos = 0;
            fdd.bufferLen = DISK_BYTES_PER_SECTOR;
            break;
        case 0xB0:
            printf ("TODO write multiple sector mark=%d\n", data&3);
            break;
        case 0xC0:
            mprintf (LVL_DISK, "read ID, tr=%d, side=%d, sec=%d\n",
                     fdd.track, fdd.side, fdd.sector);
            fdd.buffer = diskId;
            diskId[0] = fdd.track;
            diskId[1] = fdd.side;
            diskId[2] = fdd.sector;
            diskId[3] = 1; // statusRegister.sectorLengthCode;
            diskId[4] = 0; // crc1
            diskId[5] = 0; // crc2
            fdd.bufferPos = 0;
            fdd.bufferLen = 6;
            break;
        case 0xD0:
            /*   8 = issue interrupt now
             *   4 = interrupt at next index pulse
             *   2 = int when next ready
             *   1 = int when next not ready
             *   0 = no interrupt
             */
            mprintf (LVL_DISK, "force interrupt %x\n", data&0xf);
            break;
        case 0xE0:
            mprintf (LVL_DISK, "TODO read track, sync=%d\n", data&1);
            break;
        case 0xF0:
            mprintf (LVL_DISK, "write track\n");
            fdd.buffer = NULL; // We don't care about format data
            fdd.status |= DISK_STATUS_DRQ;
            break;
        }
        break;
    case 0xA:
        mprintf (LVL_DISK, "DSK - request track %02X\n", data);
        fdd.track = data;
        break;
    case 0xC:
        mprintf (LVL_DISK, "DSK - request sector %02X\n", data);
        fdd.sector = data;
        break;
    case 0xE:
        /*  If we have a buffer then put the data into that otherwise put it in
         *  the data register.
         */
        if (fdd.buffer)
        {
            /*  For debug, we just output the first 4 bytes written */
            // if (fdd.bufferPos<4)
                mprintf (LVL_DISK, "DSK - write data [%02X]=%02X\n",
                fdd.bufferPos,data);

            fdd.buffer[fdd.bufferPos++] = data;

            if (fdd.bufferPos == fdd.bufferLen)
            {
                mprintf (LVL_DISK, "DSK - write finished\n");
                seekDisk();

                if (driveHandler[fdd.unit].write)
                    driveHandler[fdd.unit].write (diskSector);

                fdd.bufferPos = 0;
                fdd.buffer = NULL;
            }
        }
        else
        {
            fdd.data = data;
        }
        break;
    default:
        printf ("DSK - unknown addr\n");
        halt ("disk address");
        break;
    }
}

/*  CRU bits for motor strobe, head load pin and signal head are trapped and
 *  print informational messages, but have no functionality here.
 */
static bool fddSetStrobeMotor(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set strobe motor %d\n", state);
    fdd.motorStrobe = state;
    return false;
}

static bool fddSetIgnoreIRQ(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set ignore IRQ %d\n", state);
    fdd.ignoreIRQ = state;
    return false;
}

static bool fddSetSignalHead(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set signal head %d\n", state);
    return false;
}

/*  Selecting a drive opens a sector dump file, or closes it if the currently
 *  selected drive is deselected
 */
static bool fddSetSelectDrive(int index, uint8_t state)
{
    index -= 0x883;

    if (index < 1 || index > DISK_DRIVE_COUNT)
        halt ("disk drive select");

    mprintf(LVL_DISK, "DSK drive %d selection %d\n", index, state);

    if (state)
    {
        if (fdd.unit != index)
        {
            fdd.unit = index;

            if (driveHandler[index].select)
                driveHandler[index].select (driveHandler[index].name,
                driveHandler[index].readOnly);
        }
    }
    else if (index == fdd.unit)
    {
        if (driveHandler[index].deselect)
            driveHandler[index].deselect ();

        fdd.unit=0;
        mprintf(LVL_DISK, "DSK drive %d deselected\n", index);
    }
    return false;
}

static bool fddSetSelectSide(int index, uint8_t state)
{
    fdd.side = state;
    mprintf(LVL_DISK, "DSK set side %d\n", fdd.side);
    return false;
}

static uint8_t fddGetHLDPin (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get HLD pin\n");
    return 0;
}

static uint8_t fddGetDriveSelected (int index, uint8_t state)
{
    int unit = index - 0x880;
    mprintf(LVL_DISK, "DSK test if unit %d active\n", unit);
    return fdd.unit == unit;
}

static uint8_t fddGetMotorStrobeOn (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get motor strobe %d\n", fdd.motorStrobe);
    return fdd.motorStrobe;
}

static uint8_t fddGetSide (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get side %d\n", fdd.side);
    return fdd.side;
}

void fddRegisterHandler (int unit, fddHandler *handler)
{
    if (unit < 1 || unit > DISK_DRIVE_COUNT)
        halt ("invalid unit for handler");

    driveHandler[unit] = *handler;
    mprintf(LVL_DISK, "Handler registered for unit %d\n", unit);
}

void fddInit (void)
{
    cruOutputCallbackSet (0x880, memDeviceRomSelect);
    cruOutputCallbackSet (0x881, fddSetStrobeMotor);
    cruOutputCallbackSet (0x882, fddSetIgnoreIRQ);
    cruOutputCallbackSet (0x883, fddSetSignalHead);
    cruOutputCallbackSet (0x884, fddSetSelectDrive);
    cruOutputCallbackSet (0x885, fddSetSelectDrive);
    cruOutputCallbackSet (0x886, fddSetSelectDrive);
    cruOutputCallbackSet (0x887, fddSetSelectSide);
    cruReadCallbackSet (0x880, fddGetHLDPin);
    cruReadCallbackSet (0x881, fddGetDriveSelected);
    cruReadCallbackSet (0x882, fddGetDriveSelected);
    cruReadCallbackSet (0x883, fddGetDriveSelected);
    cruReadCallbackSet (0x884, fddGetMotorStrobeOn);
    cruReadCallbackSet (0x887, fddGetSide);
}
