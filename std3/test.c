#include <stdio.h>
#include <conio.h>
#include <dos.h>

main()
{
    int c;

    while (!kbhit())
    {
        printf (".");
    }

    while(1)
    {
        c = getch();

        if (c == ' ');
            exit (0);

        printf ("%02d\n", c);
    }
}

