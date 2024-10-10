/*
 * Copyright (c) 2004-2024 Mark Burkley.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 *  disasm.c  - standalone TMS9900 disassembler.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "mltt-disasm.h"
// #include "trace.h"
#include "parse.h"
// #include "unasm.h"
// #include "mem.h"
// #include "cpu.h"

int main (int argc, char *argv[])
{
    int addr;
    uint16_t pc;
    // TMS9900 cpu;
    Disasm disasm;

    if (argc < 3 || !parseValue (argv[2], &addr))
    {
        printf ("Usage: %s <filename> <address> [<comments-file>]\n", argv[0]);
        exit (1);
    }

    int len = memLoad (argv[1], addr, 0);

    if (argc > 3)
        disasm.unasmReadText (argv[3]);

    pc = addr;
    // outputLevel = LVL_UNASM;

    while (pc < addr+len)
    {
        uint16_t data = memReadW (pc);
        pc += 2;
        uint16_t type;
        uint16_t opcode = disasm.decode (data, &type);
        uint16_t paramWords = disasm.unasmPreExec (pc, data, type, opcode);

        disasm.unasmPostPrint ();
        // printf ("\n");

        while (paramWords)
        {
            paramWords-=2;
            data = memReadW (pc);
            printf ("%04X:%04X\n", pc, data);
            pc += 2;
        }
    }

    return 0;
}


