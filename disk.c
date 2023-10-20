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
 *  Implements floppy disk controller
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ti994a.h"
#include "cpu.h"
#include "disk.h"
#include "cru.h"
#include "interrupt.h"
#include "trace.h"

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
static FILE *diskFile;
static int sectorsPerTrack = 9;
static int tracksPerDisk = 40;
static int bytesPerSector = 256;

static struct
{
    uint8_t command;
    uint8_t *dataPtr;
    int dataLen;
    int dataPos;
    uint8_t status;
    bool direction; // true = inward
    bool ignoreIRQ;
    uint8_t sectorRequest;
    uint8_t trackRequest;
    uint8_t sectorActual;
    uint8_t trackActual;
    uint8_t driveSelected;
    uint8_t sideSelected;
    bool motorStrobe;
} diskRegister;

static bool seekActive;
static char diskFileName[3][20];

static void fileReadSector (void)
{
    int offset = diskRegister.trackActual * sectorsPerTrack + diskRegister.sectorActual;
    mprintf (LVL_DISK, "DSK - read file offset %d\n", offset);
    fseek (diskFile, bytesPerSector * offset, SEEK_SET);
    fread (diskSector, bytesPerSector, 1, diskFile);
}

static void fileWriteSector (void)
{
    int offset = diskRegister.trackActual * sectorsPerTrack + diskRegister.sectorActual;
    mprintf (LVL_DISK, "DSK - write file offset %d\n", offset);
    fseek (diskFile, bytesPerSector * offset, SEEK_SET);
    fwrite (diskSector, bytesPerSector, 1, diskFile);
}

uint16_t diskRead (uint16_t addr, uint16_t size)
{
    uint16_t value;
    uint8_t data;

    switch(addr)
    {
    case 0:
        mprintf (LVL_DISK, "DSK - read status=%02X\n", diskRegister.status);
        value = diskRegister.status;

        if (seekActive)
        {
            mprintf (LVL_DISK, "DSK - seek done, track=%d\n", diskRegister.trackRequest);
            diskRegister.trackActual = diskRegister.trackRequest;
            diskRegister.status &= ~DISK_STATUS_BUSY;
            // The ISR doesn't seem to exist and therefore doesn't reset the
            // device IRQ so disabling all ints for now
            #if 0
            if (!diskRegister.ignoreIRQ)
            {
                mprintf (LVL_DISK, "DSK - raise interrupt\n");
                cruBitInput (0, IRQ_DEVICE, 0);
            }
            #endif
            seekActive = false;

            if (diskRegister.trackActual == 0)
                diskRegister.status |= DISK_STATUS_TRACK0;
        }

        return ~value;
        break;
    case 2:
        mprintf (LVL_DISK, "DSK - read track=%02X\n", diskRegister.trackActual);
        return ~diskRegister.trackActual;
        break;
    case 4:
        mprintf (LVL_DISK, "DSK - read sector=%02X\n", diskRegister.sectorActual);
        return ~diskRegister.sectorActual;
        break;
    case 6:
        if (diskRegister.dataPtr)
            data = diskRegister.dataPtr[diskRegister.dataPos++];
        else
            data = 0xFF;

        if (diskRegister.dataPos<6)
        mprintf (LVL_DISK, "DSK - read data=%02X\n", data);

        if (diskRegister.dataPos == diskRegister.dataLen)
        {
            mprintf (LVL_DISK, "DSK - read finished\n");
            diskRegister.dataPos = 0;
            diskRegister.dataPtr = NULL;
        }
        return ~data;
    default:
        mprintf (LVL_DISK, "DSK - unknown data\n");
        break;
    }
    return 0;
}

static void trackUpdate (bool inward, bool update)
{
    if (inward)
    {
        if (diskRegister.trackRequest < tracksPerDisk)
            diskRegister.trackRequest++;
    }
    else
    {
        if (diskRegister.trackRequest > 0)
            diskRegister.trackRequest--;
    }

    if (update)
        diskRegister.trackActual = diskRegister.trackRequest;
}

void diskWrite (uint16_t addr, uint16_t data, uint16_t size)
{
    /*  Data bus is inverted in FD1771 */
    data = (~data & 0xFF);

    diskRegister.status = 0;

    switch(addr)
    {
    case 0x8:
        mprintf (LVL_DISK, "DSK - write cmd %02X : ", data);

        if (data < 0x80)
            mprintf (LVL_DISK, "speed %d, %s, %s, ", data&3, data&4?"verify":"no-verify",
                    data&8?"load" : "no-load");

        if (data >= 0x80 && data < 0xc0)
            mprintf (LVL_DISK, "%s, %s, ", data&8?"IBM":"non-IBM", data&4?"HLD":"no-delay");

        switch (data & 0xF0)
        {
        case 0x00:
            mprintf (LVL_DISK, "restore\n");
            diskRegister.trackActual = 0;
            diskRegister.trackRequest = 0;
            diskRegister.direction = true;
            diskRegister.status |= DISK_STATUS_BUSY;
            seekActive = true;
            break;
        case 0x10:
            mprintf (LVL_DISK, "seek T=%d, S=%d\n", diskRegister.trackRequest, diskRegister.sectorRequest);
            diskRegister.trackActual = diskRegister.trackRequest;
            diskRegister.sectorActual = diskRegister.sectorRequest;
            diskRegister.status |= DISK_STATUS_BUSY;
            seekActive = true;
            break;
        case 0x20:
            mprintf (LVL_DISK, "step, no upd track\n");
            trackUpdate (diskRegister.direction, false);
            break;
        case 0x30:
            mprintf (LVL_DISK, "step, upd track\n");
            trackUpdate (diskRegister.direction, true);
            break;
        case 0x40:
            mprintf (LVL_DISK, "step in, no upd track\n");
            trackUpdate (true, false);
            break;
        case 0x50:
            mprintf (LVL_DISK, "step in, upd track\n");
            trackUpdate (true, true);
            break;
        case 0x60:
            mprintf (LVL_DISK, "step out, no upd track\n");
            trackUpdate (false, false);
            break;
        case 0x70:
            mprintf (LVL_DISK, "step out, upd track\n");
            trackUpdate (false, true);
            break;
        case 0x80:
            mprintf (LVL_DISK, "read single sector\n");
            fileReadSector();
            diskRegister.dataPtr = diskSector;
            diskRegister.dataPos = 0;
            diskRegister.dataLen = bytesPerSector;
            break;
        case 0x90: mprintf (LVL_DISK, "read multiple sector\n"); break;
        case 0xA0:
            mprintf (LVL_DISK, "write single sector mark=%d\n", data&3);
            diskRegister.dataPtr = diskSector;
            diskRegister.dataPos = 0;
            diskRegister.dataLen = bytesPerSector;
            break;
        case 0xB0: mprintf (LVL_DISK, "write multiple sector mark=%d\n", data&3); break;
        case 0xC0:
            mprintf (LVL_DISK, "read ID, tr=%d, side=%d, sec=%d\n",
            diskRegister.trackActual, diskRegister.sideSelected,
            diskRegister.sectorActual);
            diskRegister.dataPtr = diskId;
            diskId[0] = diskRegister.trackActual;
            diskId[1] = diskRegister.sideSelected;
            diskId[2] = diskRegister.sectorActual;
            diskId[3] = 1; // statusRegister.sectorLengthCode;
            diskId[4] = 0; // crc1
            diskId[5] = 0; // crc2
            diskRegister.dataPos = 0;
            diskRegister.dataLen = 6;
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
        case 0xE0: mprintf (LVL_DISK, "read track, sync=%d\n", data&1); break;
        case 0xF0:
            mprintf (LVL_DISK, "write track\n");
            diskRegister.dataPtr = NULL; // We don't care about format data
            diskRegister.status |= DISK_STATUS_DRQ;
            break;
        }
        // diskRegister.status |= DISK_STATUS_HEAD_ENGAGED;
        // diskRegister.status |= DISK_STATUS_BUSY;

        break;
    case 0xA:
        mprintf (LVL_DISK, "DSK - request track %02X\n", data);
        diskRegister.trackRequest = data;
        break;
    case 0xC:
        mprintf (LVL_DISK, "DSK - request sector %02X\n", data);
        diskRegister.sectorRequest = data;
        break;
    case 0xE:
        if (diskRegister.dataPos<4)
            mprintf (LVL_DISK, "DSK - write data %02X\n", data);

        if (diskRegister.dataPtr)
        {
            diskRegister.dataPtr[diskRegister.dataPos++] = data;

            if (diskRegister.dataPos == diskRegister.dataLen)
            {
                mprintf (LVL_DISK, "DSK - write finished\n");
                fileWriteSector();
                diskRegister.dataPos = 0;
                diskRegister.dataPtr = NULL;
            }
        }
        else
        {
            // No sector write is active, put data into track request register
            // for seek
            diskRegister.trackRequest = data;
            mprintf (LVL_DISK, "DSK - track request %02X\n", data);
        }
        // diskRegister.data[diskRegister.dataPos++] = data;
        // diskInterrupt(); // yeah that's done
        // cruBitInput (0, IRQ_DEVICE, 0);
        break;
    default: mprintf (LVL_DISK, "DSK - unknown data\n"); break;
    }
}

static bool diskSetStrobeMotor(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set strobe motor %d\n", state);
    diskRegister.motorStrobe = state;
    return false;
}

static bool diskSetIgnoreIRQ(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set ignore IRQ %d\n", state);
    diskRegister.ignoreIRQ = state;
    return false;
}

static bool diskSetSignalHead(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set signal head %d\n", state);
    return false;
}

static bool diskSetSelectDrive(int index, uint8_t state)
{
    int drive = index - 0x883;

    mprintf(LVL_DISK, "DSK drive %d selection %d\n", drive, state);

    if (state)
    {
        diskRegister.driveSelected = drive;

        /*  TODO how to select no disk ? */
        if ((diskFile = fopen (diskFileName[drive-1], "r+")) == NULL)
            halt ("diskSelectDrive");
    }
    else if (drive == diskRegister.driveSelected)
    {
        diskRegister.driveSelected=0;
        mprintf(LVL_DISK, "DSK drive %d deselected\n", drive);
        fclose (diskFile);
        diskFile = NULL;
    }
    return false;
}

static bool diskSetSelectSide(int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK set side %d\n", diskRegister.sideSelected);
    diskRegister.sideSelected = state;
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
    return diskRegister.driveSelected == drive;
}

static uint8_t diskGetMotorStrobeOn (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get motor strobe %d\n", diskRegister.motorStrobe);
    return diskRegister.motorStrobe;
}

static uint8_t diskGetSide (int index, uint8_t state)
{
    mprintf(LVL_DISK, "DSK get side %d\n", diskRegister.sideSelected);
    return diskRegister.sideSelected;
}

void diskLoad (int drive, char *name)
{
    /*  Open the file with read with update.  If that fails, create the fle.  If
     *  that fails, halt */
    FILE *fp;

    strcpy (diskFileName[drive-1], name);

    if ((fp = fopen (name, "r+")) == NULL)
    {
        printf ("Create new disk file '%s'\n", name);
        if ((fp = fopen (name, "w+")) == NULL)
            halt ("disk load");
    }

    fclose (fp);
}

void diskInit (void)
{
    cruOutputCallbackSet (0x880, ti994aDeviceRomSelect);
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
