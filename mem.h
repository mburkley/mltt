#ifndef __MEM_H
#define __MEM_H

#include "cpu.h"

WORD memReadW(WORD addr);
void memWriteW(WORD addr, WORD data);
WORD memReadB(WORD addr);
void memWriteB(WORD addr, BYTE data);
WORD * memWordAddr (WORD addr);
void memLoad (char *file, WORD addr, WORD length);

#endif

