#ifndef __SOUND_H
#define __SOUND_H

#include "cpu.h"

void soundInit (void);
void soundUpdate (void);
int soundRead (int addr, int size);
void soundWrite (int addr, int data, int size);

#endif

