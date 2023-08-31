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

#ifndef __UNASM_H
#define __UNASM_H

#include <stdbool.h>

void unasmRunTimeHookAdd (void);
void unasmReadText (const char *textFile);

extern void (*unasmPreExecHook)(WORD pc, WORD data, WORD opMask, int type,
                     WORD sMode, WORD sReg, WORD sArg,
                     WORD dMode, WORD dReg, WORD dArg,
                     WORD count, WORD offset);
extern void (*unasmPostExecHook)(WORD pc, int type, bool isByte, bool store, WORD sMode,
                          WORD sAdddr, WORD dMode, WORD dReg, WORD addr,
                          WORD data, WORD regData);

#endif

