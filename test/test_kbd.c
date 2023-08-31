#include "../kbd.c"
#include "../trace.c"

static int expect (int expectRow, int expectCol, int expectVal)
{
    int row, col;
    int success = 1;

    for (row = 0; row < KBD_ROW; row++)
        for (col = 0; col < KBD_COL; col++)
        {
            int val = kbdGet (col, row);
            if (row == expectRow && col == expectCol)
            {
                if (expectVal != val)
                    success = 0;
            }
            else
            {
                if (val != 0)
                    success = 0;
            }
            printf ("%d,%d=%d\n", row, col, val);
        }

    return success;
}

int main (void)
{
    // pipe

    struct input_event ev;
    int success;

    outputLevel = 0x40;

    printf ("1..4\n");
    ev.type = EV_KEY;
    ev.value = 1;
    ev.code = 7;

    decodeEvent (ev);

    success = expect (3, 3, 1);
    printf ("%sok 1 code 7 = 3,3\n", success?"":"not ");

    ev.type = EV_KEY;
    ev.value = 0;
    ev.code = 7;

    decodeEvent (ev);
    success = expect (3, 3, 0);
    printf ("%sok 2 code 7 = 3,3\n", success?"":"not ");

    ev.type = EV_KEY;
    ev.value = 1;
    ev.code = 49;

    decodeEvent (ev);

    success = expect (0, 4, 1);
    printf ("%sok 3 code 49 = 0,4\n", success?"":"not ");

    ev.type = EV_KEY;
    ev.value = 0;
    ev.code = 49;

    decodeEvent (ev);
    success = expect (0, 4, 0);

    printf ("%sok 4 code 49 = 0,4\n", success?"":"not ");

    return 0;
}

