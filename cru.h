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

#ifndef __CRU_H
#define __CRU_H

#include "cpu.h"

void cruBitInput (uint16_t base, int8_t bitOffset, uint8_t state);
void cruBitOutput (uint16_t base, int8_t bitOffset, uint8_t state);
void cruBitInput (uint16_t base, int8_t bitOffset, uint8_t state);
uint8_t cruBitGet (uint16_t base, int8_t bitOffset);
void cruMultiBitSet (uint16_t base, uint16_t data, int nBits);
uint16_t cruMultiBitGet (uint16_t base, int nBits);
void cruInputCallbackSet (int index, bool (*callback) (int index, uint8_t state));
void cruOutputCallbackSet (int index, bool (*callback) (int index, uint8_t state));
void cruReadCallbackSet (int index, uint8_t (*callback) (int index, uint8_t state));

#endif

