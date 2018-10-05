#ifndef __VDP_H
#define __VDP_H

#include "cpu.h"

BYTE vdpReadData (void);
BYTE vdpReadStatus (void);
void vdpWriteData (BYTE data);
void vdpWriteCommand (BYTE data);
void vdpInitGraphics (void);
void vdpRefresh (int force);

#endif

