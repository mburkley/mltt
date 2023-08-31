#ifndef __TRACE_H
#define __TRACE_H

#define LVL_CONSOLE     0x001
#define LVL_CPU         0x002
#define LVL_VDP         0x004
#define LVL_GROM        0x008
#define LVL_UNASM       0x010
#define LVL_CRU         0x020
#define LVL_KBD         0x040
#define LVL_SOUND       0x080
#define LVL_GPL         0x100
#define LVL_GPLDBG      0x200

int mprintf (int level, char *s, ...);
void halt (char *s);
extern int outputLevel;

#endif

