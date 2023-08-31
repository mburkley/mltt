#ifndef __VDP_H
#define __VDP_H

#include "cpu.h"

int vdpRead (int addr, int size);
void vdpWrite (int addr, int data, int size);
void vdpInitGraphics (void);
void vdpRefresh (int force);

#endif

