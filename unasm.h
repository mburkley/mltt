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

extern void (*unasmPreExecHook)(uint16_t pc, uint16_t data, uint16_t opMask, int type,
                     uint16_t sMode, uint16_t sReg, uint16_t sArg,
                     uint16_t dMode, uint16_t dReg, uint16_t dArg,
                     uint16_t count, uint16_t offset);
extern void (*unasmPostExecHook)(uint16_t pc, int type, bool isByte, bool store, uint16_t sMode,
                          uint16_t sAdddr, uint16_t dMode, uint16_t dReg, uint16_t addr,
                          uint16_t data, uint16_t regData);

#endif

