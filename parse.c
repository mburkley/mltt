#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "parse.h"

/*  Safely parses a value from a string.  Value can be hex, decimal, octal, etc.
 *  Returns true if parsed successfully.
 */
bool parseValue (char *s, int *result)
{
    int base = 0;

    /*  If TI hex notation (>) is used or if x on its own is used then force
     *  base 16.  Otherwise let strtoul figure it out.
     */
    if (s[0] == '>' || s[0] == 'x')
    {
        s++;
        base = 16;
    }

    /*  Set errno to zero first and check it afterwards.  Slight safer than checking the
     *  return value
     */
    errno = 0;
    unsigned long conversion = strtoul (s, NULL, base);
    if (errno != 0)
        return false;

    *result = (int) conversion;
    return true;
}


