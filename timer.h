#ifndef __TIMER_H
#define __TIMER_H

void timerStart (int msec, void (*callback)(void));
void timerStop (void);
void timerPoll (void);

#endif

