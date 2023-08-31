#ifndef __UNASM_H
#define __UNASM_H

void unasmRunTimeHookAdd (void);
void unasmReadText (const char *textFile);

extern void (*unasmPreExecHook)(WORD pc, WORD data, WORD opMask, int type,
                     WORD sMode, WORD sReg, WORD sArg,
                     WORD dMode, WORD dReg, WORD dArg,
                     WORD count, WORD offset);
extern void (*unasmPostExecHook)(WORD pc, int type, BOOL isByte, BOOL store, WORD sMode,
                          WORD sAdddr, WORD dMode, WORD dReg, WORD addr,
                          WORD data, WORD regData);

#endif

