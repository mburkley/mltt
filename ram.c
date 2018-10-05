
#include "cpu.h"

typedef union
{
    // WORD w[32768];
    // BYTE b[65536];
    WORD w[0x7fff];
    BYTE b[0xffff];
}
ram;


