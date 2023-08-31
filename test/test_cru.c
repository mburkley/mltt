#include "../trace.c"
#include "../cru.c"

static int getBits (int addr, int count)
{
    int result = 0;

    for (int i = 0; i < count; i++)
    {
        if (cruBitGet (addr, i))
            result |= (1<<i);
    }

    printf ("    %s %x\n", __func__, result);
    return result;
}

int main (void)
{
    printf ("1..13\n");
    printf ("    Set bit 0 to 1\n");
    cruBitSet (0, 0, 1);

    printf ("    Set bit 0x104 to 1\n");
    cruBitSet (0x200, 0x4, 1);

    printf ("    Set bit 0xFC to 1\n");
    cruBitSet (0x200, -0x4, 1);

    printf ("    Get bit 0xFC\n");

    if (cruBitGet (0x1F8, 0))
        printf ("ok 1: bit set\n");
    else
        printf ("not ok 1: bit not set\n");

    printf ("    Get bit 0x100\n");
    if (!cruBitGet (0x200, 0))
        printf ("ok 2: bit not set\n");
    else
        printf ("not ok 2: bit set\n");

    printf ("    Get bit 0x104 positive rel\n");
    if (cruBitGet (0x200, 4))
        printf ("ok 3: bit set\n");
    else
        printf ("not ok 3: bit not set\n");

    printf ("    Get bit 0x104 negative rel\n");
    if (cruBitGet (0x210, -4))
        printf ("ok 4: bit set\n");
    else
        printf ("not ok 4: bit not set\n");

    cruMultiBitSet (0x300, 0xAA, 8);
    if (getBits (0x300, 16) == 0xAA)
        printf ("ok 5: bit set\n");
    else
        printf ("not ok 5: bit not set\n");

    if (cruMultiBitGet (0x300, 8) == 0xAA)
        printf ("ok 6: bit set\n");
    else
        printf ("not ok 6: bit not set\n");

    cruMultiBitSet (0x400, 0xAA, 4);
    if (getBits (0x400, 16) == 0xA)
        printf ("ok 7: bit set\n");
    else
        printf ("not ok 7: bit not set\n");

    if (cruMultiBitGet (0x400, 4) == 0xA)
        printf ("ok 8: bit set\n");
    else
        printf ("not ok 8: bit not set\n");

    if (cruMultiBitGet (0x400, 3) == 0x2)
        printf ("ok 9: bit set\n");
    else
        printf ("not ok 9: bit not set\n");

    cruMultiBitSet (0x500, 0x555, 12);
    if (getBits (0x500, 16) == 0x555)
        printf ("ok 10: bit set\n");
    else
        printf ("not ok 10: bit not set\n");

    if (cruMultiBitGet (0x500, 12) == 0x555)
        printf ("ok 11: bit set\n");
    else
        printf ("not ok 11: bit not set\n");

    if (cruMultiBitGet (0x500, 5) == 0x15)
        printf ("ok 12: bit set\n");
    else
        printf ("not ok 12: bit not set\n");

    cruMultiBitSet (0x600, 0xA5A5, 0);
    if (getBits (0x600, 16) == 0xA5A5)
        printf ("ok 13: bit set\n");
    else
        printf ("not ok 13: bit not set\n");
}



