#include <stdio.h>
#include <stdlib.h>

#include "parse.h"

int main (int argc, char *argv[])
{
    FILE *fp;
    int offset;
    int length;

    if (argc < 4 ||
        !parseValue (argv[2], &offset) ||
        !parseValue (argv[3], &length))
    {
        printf ("\nVery primitive command line hex editor for binary files\n\n");
        printf("Usage: %s <file-name> <offset> <length> [<replace-byte> ... ]\n\n", argv[0]);
        exit (1);
    }

    if ((fp = fopen (argv[1], "r+")) == NULL)
    {
        printf ("Can't open %s\n", argv[1]);
        exit (1);
    }

    fseek (fp, offset, SEEK_SET);

    printf ("%04X :", offset);

    for (int i = 0; i < length; i++)
    {
        int c;
        fread (&c, 1, 1, fp);
        printf (" %02X", c);
    }

    fseek (fp, offset, SEEK_SET);

    for (int i = 4; i < argc; i++)
    {
        int c;

        if (!parseValue (argv[i], &c))
        {
            printf ("Can't parse '%s'\n", argv[i]);
            exit (1);
        }

        fwrite (&c, 1, 1, fp);
    }

    printf("\n");
    return 0;
}

