main()
{
    int x = -15;

    printf ("%08X, %08X", x, x >> 1);
    printf ("%08X, %08X", x, (unsigned) x >> 1);
}
