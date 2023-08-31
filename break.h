#ifndef __BREAK_H
#define __BREAK_H

#include "cpu.h"

void breakPointAdd (WORD addr);
void breakPointList (void);
void breakPointRemove (WORD addr);
void breakPointCondition (WORD addr);
int breakPointHit (WORD addr);

#endif

