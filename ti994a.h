#ifndef __TI994A_H
#define __TI994A_H

#include "cpu.h"

WORD memRead(WORD addr, int size);

void ti994aRun (void);
void ti994aInit (void);
void ti994aClose (void);
void ti994aShowScratchPad (bool showGplUsage);
void ti994aMemLoad (char *file, WORD addr, WORD length);

#endif

