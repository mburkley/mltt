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

#ifndef __DISK_H
#define __DISK_H

#include <stdint.h>
#include <stdbool.h>

#define DISK_DRIVE_COUNT        3
#define DISK_FILENAME_MAX       1024
#define DISK_TRACKS_PER_SIDE 40
#define DISK_BYTES_PER_SECTOR 256
#define SECTORS_PER_DISK         720

typedef struct
{
    void (*seek) (int sector);
    void (*read) (unsigned char *buffer);
    void (*write) (unsigned char *buffer);
    void (*select) (const char *name, bool readOnly);
    void (*deselect) (void);
    bool readOnly;
    char name[DISK_FILENAME_MAX];
}
diskDriveHandler;

uint16_t diskRead (uint16_t addr, uint16_t size);
void diskWrite (uint16_t addr, uint16_t data, uint16_t size);
void diskLoad (int drive, char *name);
void diskInit (void);
void diskRegisterDriveHandler (int drive, diskDriveHandler *handler);

#endif
