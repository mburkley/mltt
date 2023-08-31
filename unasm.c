/*
 *  unasm.c  - TMS9900 disassembler.
 *
 *  Contains hooks for pre and post operation execution for disassembly
 *  at run time.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cover.h"
#include "cpu.h"
#include "mem.h"
#include "trace.h"

static const char *unasmText[65536];
static const char *currText;

void (*unasmPreExecHook)(WORD pc, WORD data, WORD opMask, int type,
                     WORD sMode, WORD sReg, WORD sArg,
                     WORD dMode, WORD dReg, WORD dArg,
                     WORD count, WORD offset);
void (*unasmPostExecHook)(WORD pc, int type, BOOL isByte, BOOL store, WORD sMode,
                          WORD sAddr, WORD dMode, WORD dReg, WORD addr,
                          WORD data, WORD wp);

extern int outputLevel;

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

static char * printOper (WORD mode, WORD reg, WORD arg)
{
    static char result[20];

    switch (mode)
    {
    case AMODE_NORMAL:
        sprintf (result, "%d", reg);
        break;

    case AMODE_INDIR:
        sprintf (result, "*%d", reg);
        break;

    case AMODE_SYM:
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

static void unasmTwoOp (WORD opCode, WORD pc, WORD sMode, WORD sReg,
                        WORD sAddr, WORD dMode, WORD dReg, WORD dAddr)
{
    char *name = "****";
    char op[20];
    char out[31];

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

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
    case OP_LDCR: name = "LDCR ";  break;
    case OP_STCR: name = "STCR ";  break;
    case OP_MPY:  name = "MPY ";  break;
    case OP_DIV:  name = "DIV ";  break;

    }

    strcpy (op, printOper (sMode, sReg, sAddr));

    sprintf (out, "%-4s  %s,%s",
             name,
             op,
             printOper (dMode, dReg, dAddr));

    mprintf (LVL_UNASM, "%-30.30s", out);
}

static void unasmOneOp (WORD opCode, WORD pc, WORD sMode, WORD sReg,
                        WORD sAddr)
{
    char *name = "****";
    char out[31];

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

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
             printOper (sMode, sReg, sAddr));

    mprintf (LVL_UNASM, "%-30.30s", out);
}

static void unasmImmed (WORD opCode, WORD pc, WORD sReg)
{
    char out[31];
    char *name = "****";
    int showOper = 1;
    int showData = 1;

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

    switch (opCode)
    {
    case OP_LI: name="LI"; break;
    case OP_AI: name="AI"; break;
    case OP_ANDI: name="ANDI"; break;
    case OP_ORI: name="ORI"; break;
    case OP_CI: name="CI"; break;
    case OP_STWP: name="STWP"; showData = 0; break;
    case OP_STST: name="STST"; showData = 0; break;
    case OP_LWPI: name="LWPI"; break;
    case OP_LIMI: name="LIMI"; break;
    case OP_RTWP: name="RTWP"; showOper = 0; break;
    }


    if (showOper)
    {
        if (showData)
        {
            sprintf (out, "%-4s  %s,>%04X",
                     name,
                     printOper (0, sReg, 0),
                     memReadW(pc));
        }
        else
        {
            sprintf (out, "%-4s  %s",
                     name,
                     printOper (0, sReg, 0));
        }
    }
    else
        sprintf (out, "%-4s",
                 name);

    mprintf (LVL_UNASM, "%-30.30s", out);
}

static void unasmJump (WORD opCode, WORD pc, I8 offset, BOOL cond)
{
    // char out[31];
    char *name = "****";
    int pcOffset = 1;

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

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
        mprintf (LVL_UNASM, "%-4s  >%04X                    %s",
                name,
                pc + (offset << 1),
                cond ? "****" : "");
    }
    else
    {
        mprintf (LVL_UNASM, "%-4s  %d                    %s",
                name,
                offset,
                cond ? "****" : "");
    }

    // mprintf (LVL_UNASM, "%-30.30s", out);
}

static void preExecHook (WORD pc, WORD data, WORD opMask, int type,
                     WORD sMode, WORD sReg, WORD sArg,
                     WORD dMode, WORD dReg, WORD dArg,
                     WORD count, WORD offset)
{
    const char *comment;
    int len;

    currText = unasmText[pc-2];

    while ((comment = parseComment ('-', &len)) != NULL)
    {
        mprintf (LVL_UNASM, ";%.*s\n", len, comment);
    }

    if (!outputCovered || !covered[pc >> 1])
    {
        mprintf (LVL_UNASM, "%04X:%04X ", pc - 2, data);

        switch (type)
        {
        case OPTYPE_IMMED:
            unasmImmed (data & opMask, pc, sReg);
            break;

        case OPTYPE_SINGLE:
            unasmOneOp (data & opMask, pc, sMode, sReg, sArg);
            break;

        case OPTYPE_SHIFT:
            unasmTwoOp (data & opMask, pc, 0, sReg, 0, 0, count, 0);
            break;

        case OPTYPE_JUMP:
            // TODO pass jump condition as a param, remove hardcoded 1
            unasmJump (data & opMask, pc, offset, 1);
            break;

        case OPTYPE_DUAL1:
            unasmTwoOp (data & 0xFC00, pc, sMode, sReg,
                        sArg, dMode, dReg, dArg);
            break;

        case OPTYPE_DUAL2:
            unasmTwoOp (data & 0xF000, pc, sMode, sReg,
                        sArg, dMode, dReg, dArg);
            break;
        }
    }
}

static void postExecHook (WORD pc, int type, BOOL isByte, BOOL store, WORD sMode,
                          WORD sAddr, WORD dMode, WORD dReg, WORD addr,
                          WORD data, WORD regData)
{
    char text[30] = "";

    if (outputCovered && covered[pc >> 1])
        return;

    covered[pc >> 1] = 1;

    if (store)
    {
        if (sMode)
        {
            if (isByte)
            {
                sprintf (text, "B:[%04X] -> ", (unsigned) sAddr);
            }
            else
            {
                sprintf (text, "W:[%04X] -> ", (unsigned) sAddr);
            }
        }

        if (type == OPTYPE_SINGLE)
        {
            if (isByte)
            {
                sprintf (text+strlen(text), "%02X",
                         (unsigned) data >> 8);
            }
            else
            {
                sprintf (text+strlen(text), "%04X",
                         (unsigned) regData);
            }
        }
        else if (dMode)
        {
            if (isByte)
            {
                sprintf (text+strlen(text), "%02X -> B:[%04X]",
                         (unsigned) data >> 8,
                         (unsigned) addr);
            }
            else
            {
                sprintf (text+strlen(text), "%04X -> W:[%04X]",
                         (unsigned) regData,
                         (unsigned) addr);
            }
        }
        else
        {
            sprintf (text+strlen(text), "(R%d -> %04X)",
                     dReg, regData);
        }
    }

    mprintf (LVL_UNASM, "%-26.26s", text);

    int len;
    const char *comment;
    int anyComment = 0;

    while ((comment = parseComment ('+', &len)) != NULL)
    {
        anyComment = 1;
        mprintf (LVL_UNASM, ";%.*s\n", len, comment);
    }

    if (!anyComment)
        mprintf (LVL_UNASM, "\n");
}

void unasmRunTimeHookAdd (void)
{
    unasmPreExecHook = preExecHook;
    unasmPostExecHook = postExecHook;
}

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

        int ix = strtoul (s, NULL, 16);
        ix %= 4096;
        if (unasmText[ix])
        {
            printf ("Already have text for %x\n", ix);
        }
        else
        {
            if (s[strlen(s)-1]=='\n')
                s[strlen(s)-1] = 0;

            unasmText[ix] = strdup (&s[5]);
            // printf ("Text added for %x\n", ix);
        }
    }

    fclose (fp);
}

