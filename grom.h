#ifndef __GROM_H
#define __GROM_H

#include "cpu.h"

int gromRead (int addr, int size);
void gromWrite (int addr, int data, int size);
void gromShowStatus (void);
void gromLoad (char *name, int start, int len);

#endif

