#include <stdio.h>
#include <dos.h>

main()
{
    union REGS regs;

    int x = -15;
    unsigned i;

    printf ("%08X, %08X", x, x >> 1);
    printf ("%08X, %08X", x, (unsigned) x >> 1);



    regs.h.ah = 0;
    regs.h.al = 0x13;
    int86 (0x10, &regs, &regs);

    for (i = 0; i < (unsigned) 320; i++)
    {
        pokeb(0xA000, i, i % 255);
    }

getch();

/*
_asm {
        push ax
        mov ah,0x0
        mov al,0x13
        int 0x10
        pop ax
    }
*/

}



