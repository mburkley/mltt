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

#ifndef __TI994A_H
#define __TI994A_H

#include <stdarg.h>

#include "cassette.h"
#include "cpu.h"
#include "mem.h"
#include "unasm.h"
#include "cru.h"

class TI994A:public TMS9900
{
public:
    //TMS9900 cpu;
    void run (int instPerInterrupt);
    void init (void);
    void close (void);
    void showScratchPad (bool showGplUsage);
    // void timerInterrupt (void);
    void clearRunFlag () { _runFlag = false; }
private:
    bool _runFlag;
    uint16_t _memReadW (uint16_t addr) { return memReadW (addr); }
    uint8_t _memReadB (uint16_t addr) { return memReadB (addr); }
    void _memWriteW (uint16_t addr, uint16_t data) { memWriteW (addr, data); }
    void _memWriteB (uint16_t addr, uint8_t data) { memWriteB (addr, data); }

    void _unasmPostText (const char *fmt, ...)
    {
        va_list ap;

        va_start (ap, fmt);
        unasmVPostText (fmt, ap);
        va_end (ap);
    }
    uint16_t _unasmPreExec (uint16_t pc, uint16_t data, uint16_t type, uint16_t opcode)
    { return unasmPreExec (pc, data, type, opcode ); }
    void _unasmPostPrint (void) { unasmPostPrint(); }

    void _cruBitOutput (uint16_t base, uint16_t offset, uint8_t state) { cruBitOutput (base, offset, state); }
    void _cruMultiBitSet (uint16_t base, uint16_t data, int nBits) { cruMultiBitSet (base, data, nBits); }
    uint16_t _cruMultiBitGet (uint16_t base, uint16_t offset) { return cruMultiBitGet (base, offset); }
    uint8_t _cruBitGet (uint16_t base, int8_t bitOffset) { return cruBitGet (base, bitOffset); }

    /*  Methods using legacy C callbacks must be declared static */
    static bool _interrupt (int index, uint8_t state);
    static Cassette _cassette;
};

#endif

