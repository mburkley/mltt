#ifndef __GPL_H
#define __GPL_H

#include "cpu.h"

void gplDisassemble (WORD addr, BYTE data);
void gplShowScratchPad (WORD addr);
WORD gplScratchPadNext (WORD addr);

#endif


