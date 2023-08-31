#ifndef __WATCH_H
#define __WATCH_H

#include "cpu.h"

void watchAdd (WORD addr);
void watchList (void);
void watchRemove (WORD addr);
void watchShow (void);

#endif

