/*
 * Copyright (c) 2004-2023 Mark Burkley.
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
 *  unasm.c  - TMS9900 disassembler.
 *
 *  Pre and post operation execution for disassembly
 *  at run time.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "types.h"
#include "cpu.h"
#include "mem.h"
#include "trace.h"
#include "parse.h"
#include "ti994a.h"
#include "unasm.h"

static const char *unasmText[0x10000];
static const char *currText;

extern int outputLevel;
static bool covered[0x8000];
static bool outputUncovered = false;
static bool skipCurrent = false;

static const char *parseComment (char type, int *len)
{
    const char *next;
    const char *ret;

    if (currText == NULL)
        return NULL;

    if (*currText != '@')
        halt ("invalid comment");

    if (*(currText+1) != type)
        return NULL;

    next = strchr ((char*)currText+2, '@');

    if (next)
        *len = next - currText - 2;
    else
        *len = strlen (currText) - 2;

    ret = currText + 2;

    currText = next;
    return ret;
}

static char * printOper (uint16_t mode, uint16_t reg, uint16_t *pc)
{
    static char result[20];
    uint16_t arg;

    switch (mode)
    {
    case AMODE_NORMAL:
        sprintf (result, "%d", reg);
        break;

    case AMODE_INDIR:
        sprintf (result, "*%d", reg);
        break;

    case AMODE_SYM:
        arg = memReadW (*pc);
        (*pc) += 2;

        if (reg)
        {
            sprintf (result, "@>%04X[%d]", arg, reg);
        }
        else
        {
            sprintf (result, "@>%04X", arg);
        }

        break;

    case AMODE_INDIRINC:
        sprintf (result, "*%d+", reg);
        break;
    }

    return result;
}

static void unasmTwoOp (uint16_t opCode, uint16_t *pc, uint16_t sMode, uint16_t sReg,
                        uint16_t dMode, uint16_t dReg)
{
    char *name = "****";
    char op[20];
    char out[31];

    switch (opCode)
    {
    case OP_SZC:  name = "SZC ";  break;
    case OP_SZCB: name = "SZCB";  break;
    case OP_S:    name = "S   ";  break;
    case OP_SB:   name = "SB  ";  break;
    case OP_C:    name = "C   ";  break;
    case OP_CB:   name = "CB  ";  break;
    case OP_A:    name = "A   ";  break;
    case OP_AB:   name = "AB  ";  break;
    case OP_MOV:  name = "MOV ";  break;
    case OP_MOVB: name = "MOVB";  break;
    case OP_SOC:  name = "SOC ";  break;
    case OP_SOCB: name = "SOCB";  break;
    case OP_SRA:  name = "SRA ";  break;
    case OP_SRC:  name = "SRC ";  break;
    case OP_SRL:  name = "SRL ";  break;
    case OP_SLA:  name = "SLA ";  break;
    case OP_COC:  name = "COC ";  break;
    case OP_CZC:  name = "CZC ";  break;
    case OP_XOR:  name = "XOR ";  break;
    case OP_XOP:  name = "XOP ";  break;
    case OP_LDCR: name = "LDCR";  break;
    case OP_STCR: name = "STCR";  break;
    case OP_MPY:  name = "MPY ";  break;
    case OP_DIV:  name = "DIV ";  break;

    }

    strcpy (op, printOper (sMode, sReg, pc));

    sprintf (out, "%-4s  %s,%s",
             name,
             op,
             printOper (dMode, dReg, pc));

    mprintf (LVL_UNASM, "%-30.30s", out);
}

static void unasmOneOp (uint16_t opCode, uint16_t *pc, uint16_t sMode, uint16_t
sReg)
{
    char *name = "****";
    char out[31];

    switch (opCode)
    {
    case OP_BLWP: name = "BLWP"; break;
    case OP_B:    name = "B";   break;
    case OP_X:    name = "X";   break;
    case OP_CLR:  name = "CLR"; break;
    case OP_NEG:  name = "NEG"; break;
    case OP_INV:  name = "INV"; break;
    case OP_INC:  name = "INC"; break;
    case OP_INCT: name = "INCT";    break;
    case OP_DEC:  name = "DEC"; break;
    case OP_DECT: name = "DECT";    break;
    case OP_BL:   name = "BL";  break;
    case OP_SWPB: name = "SWPB";    break;
    case OP_SETO: name = "SETO";    break;
    case OP_ABS:  name = "ABS"; break;
    }

    sprintf (out, "%-4s  %s",
             name,
             printOper (sMode, sReg, pc));

    mprintf (LVL_UNASM, "%-30.30s", out);
}

static void unasmImmed (uint16_t opCode, uint16_t *pc, uint16_t sReg)
{
    char out[31];
    char *name = "****";
    bool showReg = true;
    bool showOper = true;
    bool showData = true;

    switch (opCode)
    {
    case OP_LI: name="LI"; break;
    case OP_AI: name="AI"; break;
    case OP_ANDI: name="ANDI"; break;
    case OP_ORI: name="ORI"; break;
    case OP_CI: name="CI"; break;
    case OP_STWP: name="STWP"; showData = false; break;
    case OP_STST: name="STST"; showData = false; break;
    case OP_LWPI: name="LWPI"; showReg = false; break;
    case OP_LIMI: name="LIMI"; showReg = false; break;
    case OP_RTWP: name="RTWP"; showOper = false; break;
    default: showOper = false; break;
    }

    if (showOper)
    {
        if (showData)
        {

            if (showReg)
            {
                sprintf (out, "%-4s  %d,>%04X",
                         name,
                         sReg,
                         memReadW(*pc));
            }
            else
            {
                sprintf (out, "%-4s  >%04X",
                         name,
                         memReadW(*pc));
            }
            (*pc) += 2;
        }
        else
        {
            sprintf (out, "%-4s  %d",
                     name,
                     sReg);
        }
    }
    else
        sprintf (out, "%-4s",
                 name);

    mprintf (LVL_UNASM, "%-30.30s", out);
}

static void unasmJump (uint16_t opCode, uint16_t pc, int8_t offset)
{
    char out[31];
    char *name = "****";
    int pcOffset = 1;

    switch (opCode)
    {
    case OP_JMP: name="JMP"; break;
    case OP_JLT: name="JLT"; break;
    case OP_JLE: name="JLE"; break;
    case OP_JEQ: name="JEQ"; break;
    case OP_JHE: name="JHE"; break;
    case OP_JGT: name="JGT"; break;
    case OP_JNE: name="JNE"; break;
    case OP_JNC: name="JNC"; break;
    case OP_JOC: name="JOC"; break;
    case OP_JNO: name="JNO"; break;
    case OP_JL: name="JL"; break;
    case OP_JH: name="JH"; break;
    case OP_JOP: name="JOP"; break;
    case OP_SBO: name="SBO"; pcOffset = 0; break;
    case OP_SBZ: name="SBZ"; pcOffset = 0; break;
    case OP_TB: name="TB"; pcOffset = 0; break;
    }

    if (pcOffset)
    {
        sprintf (out, "%-4s  >%04X",
                name,
                pc + (offset << 1));
    }
    else
    {
        sprintf (out, "%-4s  %d",
                name,
                offset);
    }

    mprintf (LVL_UNASM, "%-30.30s", out);
}

/*  Pre execution disassembly.  Returns number of words consumed */
uint16_t unasmPreExec (uint16_t pc, uint16_t data, uint16_t type, uint16_t opcode)
{
    const char *comment;
    int len;
    int sReg = 0;
    int dReg = 0;
    int sMode = 0;
    int dMode = 0;
    uint16_t pcStart = pc;

    if (outputLevel < 2)
        return 0;

    if (covered[pc>>1])
    {
        skipCurrent = true;
        return 0;
    }

    if (outputUncovered)
    {
        covered[pc>>1] = true;
        skipCurrent = false;
    }

    currText = unasmText[pc-2];

    while ((comment = parseComment ('-', &len)) != NULL)
    {
        mprintf (LVL_UNASM, ";%.*s\n", len, comment);
    }

    mprintf (LVL_UNASM, "%04X:%04X ", pc-2, data);

    switch (type)
    {
    case OPTYPE_IMMED:
        sReg   =  data & 0x000F;
        unasmImmed (opcode, &pc, sReg);
        break;

    case OPTYPE_SINGLE:
        sMode = (data & 0x0030) >> 4;
        sReg     =  data & 0x000F;
        unasmOneOp (opcode, &pc, sMode, sReg);
        break;

    case OPTYPE_SHIFT:
        dReg = (data & 0x00F0) >> 4;
        sReg =  data & 0x000F;
        unasmTwoOp (opcode, &pc, 0, sReg, 0, dReg);
        break;

    case OPTYPE_JUMP:
        sReg = data & 0x00FF;
        unasmJump (opcode, pc, sReg);
        break;

    case OPTYPE_DUAL1:
        dReg     = (data & 0x03C0) >> 6;
        sMode = (data & 0x0030) >> 4;
        sReg     =  data & 0x000F;
        unasmTwoOp (opcode, &pc, sMode, sReg, 0, dReg);
        break;

    case OPTYPE_DUAL2:
        dMode = (data & 0x0C00) >> 10;
        dReg     = (data & 0x03C0) >> 6;
        sMode = (data & 0x0030) >> 4;
        sReg     =  data & 0x000F;
        unasmTwoOp (opcode, &pc, sMode, sReg, dMode, dReg);
        break;
    }

    return pc - pcStart;
}

static char unasmTextBuffer[100];
static char *unasmTextPtr = unasmTextBuffer;

void unasmPostText (char *fmt, ...)
{
    va_list ap;

    if (skipCurrent)
        return;

    va_start (ap, fmt);
    int len = vsprintf (unasmTextPtr, fmt, ap);
    unasmTextPtr += len;
    va_end (ap);
}

void unasmPostPrint (void)
{
    const char *comment;
    int anyComment = 0;
    int len;

    if (skipCurrent)
        return;

    mprintf (LVL_UNASM, "%-40.40s", unasmTextBuffer);
    unasmTextPtr = unasmTextBuffer;
    unasmTextBuffer[0] = 0;

    while ((comment = parseComment ('+', &len)) != NULL)
    {
        anyComment = 1;
        mprintf (LVL_UNASM, ";%.*s\n", len, comment);
    }

    if (!anyComment)
        mprintf (LVL_UNASM, "\n");
}

/*  Reads comments in from a file and displays them after an instruciton that
 *  has been executed.
 *
 *  NOTE : At present, the file has no way to select ROM banks for even
 *  cartridges.  Different files should be used for different cartridges.  At
 *  the moment, unasm.txt contains comments for console ROM, disk DSR and
 *  extended basic (a mix of C and D).
 */
void unasmReadText (const char *textFile)
{
    FILE *fp;
    char s[2048];

    if ((fp = fopen (textFile, "r")) == NULL)
    {
        printf ("** Failed to open %s\n", textFile);
        return;
    }

    while (!feof (fp))
    {
        if (!fgets (s, sizeof s, fp))
            continue;

        uint16_t ix = strtoul (s, NULL, 16);
        if (unasmText[ix])
        {
            printf ("Already have text for %x\n", ix);
        }
        else
        {
            if (s[strlen(s)-1]=='\n')
                s[strlen(s)-1] = 0;

            unasmText[ix] = strdup (&s[5]);
        }
    }

    fclose (fp);
}

void unasmOutputUncovered (bool state)
{
    outputUncovered = state;
}


