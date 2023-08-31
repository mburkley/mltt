#ifndef __KBD_H
#define __KBD_H

#define KBD_ROW 8
#define KBD_COL 8

#ifdef __cplusplus
extern "C" {
#endif

void kbdPoll (void);
int kbdGet (int row, int col);
void kbdClose (void);
void kbdOpen (const char *device);

#ifdef __cplusplus
}
#endif

#endif
