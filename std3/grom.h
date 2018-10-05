#ifndef __GROM_H
#define __GROM_H

#include "cpu.h"

BYTE gromRead (void);
void gromSetAddr (WORD addr);
BYTE gromGetAddr (void);
void showGromStatus (void);
void loadGRom (void);

#endif

