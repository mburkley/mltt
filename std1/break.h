#ifndef __BREAK_H
#define __BREAK_H

#include "cpu.h"

void breakPointAdd (char *s);
void breakPointList (void);
void breakPointRemove (char *s);
int breakPointHit (WORD addr);

#endif

