/*
 *  unasm.c  - TMS9900 disassembler.
 *
 *  Contains hooks for pre and post operation execution for disassembly
 *  at run time.
 *
 */

#include <stdio.h>
#include <string.h>

#include "cover.h"
#include "cpu.h"
#include "mem.h"

#define DBG_LVL 4

void (*unasmPreExecHook)(WORD,WORD);
void (*unasmPostExecHook)(WORD,WORD);

extern int outputLevel;

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

    covered[pc >> 1] = 1;

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

    }

    strcpy (op, printOper (sMode, sReg, sAddr));

    sprintf (out, "%-4s\t%s,%s",
             name,
             op,
             printOper (dMode, dReg, dAddr));

    mprintf (4, "%-30.30s", out);
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

    covered[pc >> 1] = 1;

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
            
    sprintf (out, "%-4s\t%s",
             name,
             printOper (sMode, sReg, sAddr));

    mprintf (4, "%-30.30s", out);
}

static void unasmImmed (WORD opCode, WORD pc, WORD sReg)
{
    char out[31];
    char *name = "****";

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

    covered[pc >> 1] = 1;

    switch (opCode)
    {
    case OP_LI: name="LI"; break;
    case OP_AI: name="AI"; break;
    case OP_ANDI: name="ANDI"; break;
    case OP_ORI: name="ORI"; break;
    case OP_CI: name="CI"; break;
    case OP_STWP: name="STWP"; break;
    case OP_STST: name="STST"; break;
    case OP_LWPI: name="LWPI"; break;
    case OP_LIMI: name="LIMI"; break;
    case OP_RTWP: name="RTWP"; break;
    }

    sprintf (out, "%-4s\t%s,>%04X",
             name,
             printOper (0, sReg, 0),
             memRead(pc));

    mprintf (4, "%-30.30s", out);
}

static void unasmJump (WORD opCode, WORD pc, I8 offset, BOOL cond)
{
    char out[31];
    char *name = "****";

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

    covered[pc >> 1] = 1;

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
    }

    sprintf (out, "%-4s\t>%04X\t\t%s",
            name,
            pc + (offset << 1),
            cond ? "****" : "");

    mprintf (4, "%-30.30s", out);
}

static void unasmCRU (WORD opCode, WORD pc, I8 offset)
{
    char out[31];
    char *name = "****";

    if (outputLevel < 2)
        return;

    if (outputCovered && covered[pc >> 1])
        return;

    covered[pc >> 1] = 1;

    switch (opCode)
    {
    case OP_SBO: name="SBO"; break;
    case OP_SBZ: name="SBZ"; break;
    case OP_TB: name="TB"; break;
    }

    sprintf (out, "%-4s\t%d",
            name,
            offset);

    mprintf (4, "%-30.30s", out);
}

static void preExecHook (WORD pc, WORD data, WORD opMask, int type,
                     WORD sMode, WORD sReg, WORD sArg,
                     WORD dMode, WORD dReg, WORD dArg,
                     WORD count, WORD offset)
{
    if (!outputCovered || !covered[pc >> 1])
    {
        mprintf (4, "%04X:%04X ", pc - 2, data);

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
            unasmJump (data & opMask, pc, offset, 1);
            break;

        case OPTYPE_DUAL1:
        case OPTYPE_DUAL2:
            unasmTwoOp (data & 0xF000, pc, sMode, sReg,
                        sArg, dMode, dReg, dArg);
            break;
        }
    }
}

static void postExecHook (BOOL isByte, BOOL store, WORD dMode, WORD dReg, WORD addr,
                          WORD data, WORD wp)
{
    if (store)
    {
        if (dMode)
        {
            if (isByte)
            {
                mprintf (4, "B:[%04X] = %02X",
                        (unsigned) addr, (unsigned) data >> 8);
            }
            else
            {
                mprintf (4, "W:[%04X] = %04X",
                        (unsigned) addr, (unsigned) data);
            }
        }
        else
        {
            mprintf (4, " (R%d -> %04X)",
                     dReg, data);
        }
    }

    mprintf (4, "\n");
}

void unasmRunTimeHookAdd (void)
{
    unasmPreExecHook = preExecHook;
    unasmPostExecHook = postExecHook;
}

