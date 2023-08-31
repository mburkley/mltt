#ifndef __CRU_H
#define __CRU_H

#include "cpu.h"

void cruBitSet (WORD base, I8 bitOffset, BYTE state);
void cruBitInput (WORD base, I8 bitOffset, BYTE state);
BYTE cruBitGet (WORD base, I8 bitOffset);
void cruMultiBitSet (WORD base, WORD data, int nBits);
WORD cruMultiBitGet (WORD base, int nBits);
void cruCallbackSet (int index, void (*callback) (int index, BYTE state));

#endif

