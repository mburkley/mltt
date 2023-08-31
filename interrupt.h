#ifndef __INTERRUPT_H
#define __INTERRUPT_H

void interruptRequest (int level);
int interruptLevel (int mask);
void interruptInit(void);

#endif


