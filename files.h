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

#ifndef __FILES_H
#define __FILES_H

#include "types.h"

#define FLAG_VAR        0x80
#define FLAG_EMU        0x20
#define FLAG_MOD        0x10
#define FLAG_WP         0x08
#define FLAG_BIN        0x02
#define FLAG_PROG       0x01

#define SIZEOF_TIFILES_HEADER   0x80
#define BYTES_PER_SECTOR        0x100

typedef struct _tifiles
{
    uint8_t marker;
    char ident[7];
    // 0x08
    int16_t secCount;
    uint8_t flags;
    uint8_t recSec;
    uint8_t eofOffset;
    uint8_t recLen;
    int16_t l3Alloc;
    // 0x10
    char name[10];
    uint8_t mxt;
    uint8_t _reserved1;
    uint16_t extHeader;
    uint8_t timeCreate[4];
    uint8_t timeUpdate[4];
    uint8_t _reserved2[2];
    // 0x28
    uint8_t _reserved3[0x58];
}
Tifiles;

typedef struct _fileMetaData
{
    bool hasTifilesHeader;
    Tifiles header;
    int16_t secCount;
    int16_t l3Alloc;
    uint16_t extHeader;
    char osname[11];
}
FileMetaData;

void filesInitMeta (FileMetaData *meta, const char *name, int length, int sectorSize, int recLen, bool program, bool readOnly);
void filesInitTifiles (Tifiles *header, const char *name, int length, int
sectorSize, int recLen, bool program, bool readOnly);
int filesReadText (const char *name, char **data, bool verbose);
int filesReadBinary (const char *name, uint8_t **data, FileMetaData *meta, bool verbose);
int filesWriteText (const char *name, char *data, int length, bool verbose);
int filesWriteBinary (const char *name, uint8_t *data, int length,
                      FileMetaData *meta, bool verbose);
int filesReadTIFiles(const char *name, uint8_t *data, int maxLength);
char *filesShowFlags (uint8_t flags);
void filesLinux2TI (const char *lname, char tname[]);
void filesTI2Linux (const char *tname, char *lname);

#endif

