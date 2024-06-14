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

#include <string>

#include "types.h"

#define FLAG_VAR        0x80
#define FLAG_EMU        0x20
#define FLAG_MOD        0x10
#define FLAG_WP         0x08
#define FLAG_BIN        0x02
#define FLAG_PROG       0x01

#define SIZEOF_TIFILES_HEADER   0x80

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

class Files
{
private:
    /* The has header field is set if a TIFILES header is seen on an input file.
     * If it is set to false before a read then it is assumed the input
     * definitely does not have a TIFILES header and the methods do not look for
     * one */
    bool _hasTifilesHeader;
    Tifiles _header;
    // int16_t _secCount;
    // int16_t _l3Alloc;
    // uint16_t _extHeader;
    std::string _osname;
    uint8_t *_data;
    int _allocLen;
    int _dataLen;
    bool _verbose;
public:
    Files (const char *name, bool hasTIfiles, bool verbose);
    bool realloc (int length);
    bool hasTIfiles () { return _hasTifilesHeader; }
    void setData (uint8_t *data, int dataLen) { _data = data; _dataLen = dataLen; }
    uint8_t *getData () { return _data; }
    int getSize () { return _dataLen; }
    bool setSize (int size);
    int getFlags () { return _header.flags; }
    void setFlags (int flags) { _header.flags = flags; }
    void setRecLen (int recLen);
    int getRecLen () { return _header.recLen; }
    int getSecCount () { return _header.secCount; }
    int getRecSec () { return _header.recSec; }
    int getEofOffset () { return _header.eofOffset; }
    char *getTiname () { return _header.name; }
    // void initMeta (const char *name, int length, int sectorSize,
    //                int recLen, bool program, bool readOnly);
    void initTifiles (const char *name, int length, int sectorSize,
                      int recLen, bool program, bool readOnly);
    int read ();
    int write ();
    int readTIFiles(uint8_t *data, int maxLength);
    void getxattr (bool show);
    void setxattr ();
    static char *showFlags (uint8_t flags);
    // static void Linux2TI (const char *lname, char tname[]);
    static void Linux2TI (std::string lname, char tname[]);
    // static void TI2Linux (const char tname[], char *lname);
    static void TI2Linux (const char tname[], std::string& lname);
};


#endif

