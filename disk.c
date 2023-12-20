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
#include <stdbool.h>
#include <unistd.h>

#include "mem.h"
#include "cpu.h"
#include "disk.h"
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
static uint8_t diskSector[256];
static int sectorsPerTrack = 9;

/*  Declare one extra drive handler as drives are numbered 1 to 3 and 0 means no
 *  drive selected */
static diskDriveHandler driveHandler[DISK_DRIVE_COUNT+1];

static struct
{
    uint8_t command;
    uint8_t data;
    uint8_t sector;
    uint8_t track;
    uint8_t drive;
    uint8_t side;
    uint8_t *buffer;
    int bufferLen;
    int bufferPos;
    uint8_t status;
    bool direction; // true = inward
    bool ignoreIRQ;
    bool motorStrobe;
} disk;

static void seekDisk (void)
{
    int sector = disk.sector;

    if (disk.side)
    {
        /*  Add the offset for the first side */
        sector += sectorsPerTrack * DISK_TRACKS_PER_SIDE;

        /*  For double sided disks saved in a sector-dump (.dsk file), the track
         *  number is "inverted" so track 39 is the first track on the second
         *  side and track 0 is the last track.
         */
        sector += (DISK_TRACKS_PER_SIDE - 1 - disk.track) * sectorsPerTrack;
    }
    else
        sector += disk.track * sectorsPerTrack;

    mprintf (LVL_DISK, "DSK - access sector %d [T:%d Sec:%d Side:%d]\n", sector, 
             disk.track, disk.sector, disk.side);

    if (driveHandler[disk.drive].seek)
        driveHandler[disk.drive].seek (sector);
}

uint16_t diskRead (uint16_t addr, uint16_t size)
{
    uint8_t data;

    switch(addr)
    {
    case 0:
        mprintf (LVL_DISK, "DSK - read status=%02X\n", disk.status);
        return ~disk.status;
        break;
    case 2:
        mprintf (LVL_DISK, "DSK - read track=%02X\n", disk.track);
        return ~disk.track;
        break;
    case 4:
        mprintf (LVL_DISK, "DSK - read sector=%02X\n", disk.sector);
        return ~disk.sector;
        break;
    case 6:
        if (disk.buffer)
            data = disk.buffer[disk.bufferPos++];
        else
            data = disk.data;

        if (disk.bufferPos<6)
            mprintf (LVL_DISK, "DSK - read data=%02X\n", data);

        if (disk.bufferPos == disk.bufferLen)
        {
            mprintf (LVL_DISK, "DSK - read finished\n");
            disk.bufferPos = 0;
            disk.buffer = NULL;
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
        if (disk.track < DISK_TRACKS_PER_SIDE)
            disk.track++;
    }
    else
    {
        if (disk.track > 0)
            disk.track--;
    }
}

void diskWrite (uint16_t addr, uint16_t data, uint16_t size)
{
    /*  Data bus is inverted in FD1771 */
    data = (~data & 0xFF);

    disk.status = 0;

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
            disk.track = 0;
            disk.direction = true;
            disk.status |= DISK_STATUS_TRACK0;
            break;
        case 0x10:
            /*  "[Seek] assumes that the Track Register contains the track
             *  number of the current position of the Read-Write head and the
             *  Data Register contains the desired track number"
             *
             *  So we just copy the data register to the track register to
             *  complete the seek.
             */
            disk.track = disk.data;
            mprintf (LVL_DISK, "seek T=%d, S=%d\n", disk.track, disk.sector);
            if (disk.track == 0)
                disk.status |= DISK_STATUS_TRACK0;
            break;
        case 0x20:
        case 0x30:
            /*  The update flag is meaningless since here all seeks are
             *  instantaneous.  The track register always reflects the desired
             *  track
             */
            mprintf (LVL_DISK, "step\n");
            trackUpdate (disk.direction);
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

            if (driveHandler[disk.drive].read)
                driveHandler[disk.drive].read (diskSector);

            disk.buffer = diskSector;
            disk.bufferPos = 0;
            disk.bufferLen = DISK_BYTES_PER_SECTOR;
            break;
        case 0x90:
            printf ("TODO read multiple sector\n");
            break;
        case 0xA0:
            mprintf (LVL_DISK, "write single sector mark=%d\n", data&3);
            disk.buffer = diskSector;
            disk.bufferPos = 0;
            disk.bufferLen = DISK_BYTES_PER_SECTOR;
            break;
        case 0xB0:
            printf ("TODO write multiple sector mark=%d\n", data&3);
            break;
        case 0xC0:
            mprintf (LVL_DISK, "read ID, tr=%d, side=%d, sec=%d\n",
                     disk.track, disk.side, disk.sector);
            disk.buffer = diskId;
            diskId[0] = disk.track;
            diskId[1] = disk.side;
            diskId[2] = disk.sector;
            diskId[3] = 1; // statusRegister.sectorLengthCode;
            diskId[4] = 0; // crc1
            diskId[5] = 0; // crc2
            disk.bufferPos = 0;
            disk.bufferLen = 6;
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
            disk.buffer = NULL; // We don't care about format data
            disk.status |= DISK_STATUS_DRQ;
            break;
        }
        break;
    case 0xA:
        mprintf (LVL_DISK, "DSK - request track %02X\n", data);
        disk.track = data;
        break;
    case 0xC:
        mprintf (LVL_DISK, "DSK - request sector %02X\n", data);
        disk.sector = data;
        break;
    case 0xE:
        /*  If we have a buffer then put the data into that otherwise put it in
         *  the data register.
         */
        if (disk.buffer)
        {
            /*  For debug, we just output the first 4 bytes written */
            if (disk.bufferPos<4)
                mprintf (LVL_DISK, "DSK - write data %02X\n", data);

            disk.buffer[disk.bufferPos++] = data;

            if (disk.bufferPos == disk.bufferLen)
            {
                mprintf (LVL_DISK, "DSK - write finished\n");
                seekDisk();

                if (driveHandler[disk.drive].write)
                    driveHandler[disk.drive].write (diskSector);

                disk.bufferPos = 0;
                disk.buffer = NULL;
            }
        }
        else
        {
            disk.data = data;
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
static bool diskSetStrobeMotor(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set strobe motor %d\n", state);
    disk.motorStrobe = state;
    return false;
}

static bool diskSetIgnoreIRQ(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set ignore IRQ %d\n", state);
    disk.ignoreIRQ = state;
    return false;
}

static bool diskSetSignalHead(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set signal head %d\n", state);
    return false;
}

/*  Selecting a drive opens a sector dump file, or closes it if the currently
 *  selected drive is deselected
 */
static bool diskSetSelectDrive(int index, uint8_t state)
{
    int drive = index - 0x883;

    if (drive < 1 || drive > DISK_DRIVE_COUNT)
        halt ("disk drive select");

    mprintf(LVL_DISK, "DSK drive %d selection %d\n", drive, state);

    if (state)
    {
        if (disk.drive != drive)
        {
            disk.drive = drive;

            if (driveHandler[drive].select)
                driveHandler[drive].select (driveHandler[drive].name, driveHandler[drive].readOnly);
        }
    }
    else if (drive == disk.drive)
    {
        if (driveHandler[drive].deselect)
            driveHandler[drive].deselect ();

        disk.drive=0;
        mprintf(LVL_DISK, "DSK drive %d deselected\n", drive);
    }
    return false;
}

static bool diskSetSelectSide(int index, uint8_t state)
{
    disk.side = state;
    mprintf(LVL_DISK, "DSK set side %d\n", disk.side);
    return false;
}

static uint8_t diskGetHLDPin (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get HLD pin\n");
    return 0;
}

static uint8_t diskGetDriveSelected (int index, uint8_t state)
{
    int drive = index - 0x880;
    mprintf(LVL_DISK, "DSK test if drive %d active\n", drive);
    return disk.drive == drive;
}

static uint8_t diskGetMotorStrobeOn (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get motor strobe %d\n", disk.motorStrobe);
    return disk.motorStrobe;
}

static uint8_t diskGetSide (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get side %d\n", disk.side);
    return disk.side;
}

void diskRegisterDriveHandler (int drive, diskDriveHandler *handler)
{
    if (drive < 1 || drive > DISK_DRIVE_COUNT)
        halt ("invalid drive for handler");

    driveHandler[drive] = *handler;
    mprintf(LVL_DISK, "Handler registered for drive %d\n", drive);
}

void diskInit (void)
{
    cruOutputCallbackSet (0x880, memDeviceRomSelect);
    cruOutputCallbackSet (0x881, diskSetStrobeMotor);
    cruOutputCallbackSet (0x882, diskSetIgnoreIRQ);
    cruOutputCallbackSet (0x883, diskSetSignalHead);
    cruOutputCallbackSet (0x884, diskSetSelectDrive);
    cruOutputCallbackSet (0x885, diskSetSelectDrive);
    cruOutputCallbackSet (0x886, diskSetSelectDrive);
    cruOutputCallbackSet (0x887, diskSetSelectSide);
    cruReadCallbackSet (0x880, diskGetHLDPin);
    cruReadCallbackSet (0x881, diskGetDriveSelected);
    cruReadCallbackSet (0x882, diskGetDriveSelected);
    cruReadCallbackSet (0x883, diskGetDriveSelected);
    cruReadCallbackSet (0x884, diskGetMotorStrobeOn);
    cruReadCallbackSet (0x887, diskGetSide);
}
