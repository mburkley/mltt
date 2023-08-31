#ifndef __COND_H
#define __COND_H

#include "cpu.h"

void conditionAdd (WORD addr, char *comp, WORD value);
void conditionList (void);
void conditionRemove (WORD addr);
int conditionTrue (void);

#endif

