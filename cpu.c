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
 *  Implements TMS9900 CPU
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "vdp.h"
#include "cpu.h"
#include "grom.h"
#include "break.h"
#include "watch.h"
#include "cond.h"
#include "interrupt.h"
#include "cru.h"
#include "cover.h"
#include "unasm.h"
#include "mem.h"
#include "trace.h"

struct
{
    WORD pc;
    WORD wp;
    WORD st;
}
tms9900;

typedef struct
{
    WORD index;
    WORD type;
    WORD opMask;
    bool store;
    bool hasDest;
    bool isByte;
    bool cmpZero;
}
OpGroup;

OpGroup opGroup[64] =
{
    { 0x0000, OPTYPE_IMMED,   0xFFE0, 0, 0, 0, 1 },
    { 0x0400, OPTYPE_SINGLE,  0xFFC0, 0, 0, 0, 1 },
    { 0x0800, OPTYPE_SHIFT,   0xFF00, 0, 0, 0, 1 },
    { 0x0C00, 0,              0xFFFF, 0, 0, 0, 0 },
    { 0x1000, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x1400, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x1800, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x1C00, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x2000, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x2400, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x2800, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x2C00, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x3000, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x3400, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x3800, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x3C00, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x4000, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x4400, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x4800, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x4C00, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x5000, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x5400, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x5800, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x5C00, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x6000, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x6400, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x6800, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x6C00, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x7000, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x7400, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x7800, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x7C00, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x8000, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x8400, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x8800, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x8C00, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x9000, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x9400, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x9800, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x9C00, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xA000, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xA400, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xA800, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xAC00, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xB000, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xB400, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xB800, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xBC00, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xC000, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xC400, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xC800, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xCC00, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xD000, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xD400, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xD800, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xDC00, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xE000, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xE400, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xE800, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xEC00, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0xF000, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xF400, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xF800, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0xFC00, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 }
};

#define REGR(r) memReadW(tms9900.wp+((r)<<1))
#define REGW(r,d) memWriteW(tms9900.wp+((r)<<1),d)

WORD cpuFetch (void)
{
    WORD ret;

    ret = memReadW(tms9900.pc);
    tms9900.pc += 2;
    return ret;
}

WORD cpuGetPC (void)
{
    return tms9900.pc;
}

WORD cpuGetWP (void)
{
    return tms9900.wp;
}

WORD cpuGetST (void)
{
    return tms9900.st;
}

static void blwp (int addr)
{
    WORD owp = tms9900.wp;
    WORD opc = tms9900.pc;

    tms9900.wp = memReadW (addr);
    tms9900.pc = memReadW (addr+2);

    mprintf (LVL_CPU, "blwp @%x, wp=%x, pc=%x\n", addr, tms9900.wp, tms9900.pc);

    REGW (13, owp);
    REGW (14, opc);
    REGW (15, tms9900.st);
}

static void rtwp (void)
{
    tms9900.pc = REGR (14);
    tms9900.st = REGR (15);
    tms9900.wp = REGR (13);
}

/*  Jump if all bits in the set mask are set and all bits in the clear mask are
 *  clear */
static void jumpAnd (WORD setMask, WORD clrMask, WORD offset)
{
    if ((tms9900.st & setMask) == setMask &&
        (~tms9900.st & clrMask) == clrMask)
    {
        tms9900.pc += offset << 1;
    }
}

/*  Jump if any bits in the set mask are set or any bits in the clear mask are
 *  clear */
static void jumpOr (WORD setMask, WORD clrMask, WORD offset)
{
    if ((tms9900.st & setMask) != 0 ||
        (~tms9900.st & clrMask) != 0)
    {
        tms9900.pc += offset << 1;
    }
}

static void compare (WORD sData, WORD dData)
{
    /*
     *  Leave int. mask and Carry flag
     */

    tms9900.st &= (FLAG_MSK | FLAG_C);

    if (sData == dData)
    {
        tms9900.st |= FLAG_EQ;
    }

    if (sData > dData)
    {
        tms9900.st |= FLAG_LGT;
    }

    if ((signed short) sData > (signed short) dData)
    {
        tms9900.st |= FLAG_AGT;
    }
}

static void operand (WORD mode, WORD reg, WORD *arg, WORD *addr, bool isByte)
{
    switch (mode)
    {
    case AMODE_NORMAL:
        *addr = tms9900.wp+(reg<<1);
        break;

    case AMODE_INDIR:
        *addr = REGR(reg);
        break;

    case AMODE_SYM:
        *arg = cpuFetch();
        *addr = (WORD) (*arg + (reg == 0 ? 0 : REGR(reg)));
        break;

    case AMODE_INDIRINC:
        *addr = REGR(reg);
        REGW (reg, REGR(reg) + (isByte ? 1 : 2));
        break;
    default:
        halt ("Bad operand mode");
    }
}

static void carrySet (int condition)
{
    if (condition)
        tms9900.st |= FLAG_C;
    else
        tms9900.st &= ~FLAG_C;
}

void cpuExecute (int data)
{
    WORD  sReg = 0, dReg = 0;
    WORD sAddr = 0, dAddr = 0;
    WORD sData = 0, dData = 0;
    WORD dMode = 0, sMode = 0;
    WORD sArg = 0, dArg = 0;
    WORD pc = tms9900.pc;
    bool doStore = 0;
    bool doCmpZ = 0;
    I8 offset = 0;
    WORD count = 0;
    U32 u32 = 0;
    I32 i32 = 0;
    OpGroup *o = &opGroup[data >> 10];
    int isByte = o->isByte;

    switch (o->type)
    {
    case OPTYPE_IMMED:
        sReg   = data & 0x000F;
        break;

    case OPTYPE_SINGLE:
        sMode  = (data & 0x0030) >> 4;
        sReg   =  data & 0x000F;
        break;

    case OPTYPE_SHIFT:
        count  = (data & 0x00F0) >> 4;

        if (count == 0)
            count = REGR(0) & 0x000F;

        if (count == 0)
            count = 16;

        sReg   =  data & 0x000F;
        break;

    case OPTYPE_JUMP:
        offset = data & 0x00FF;
        break;

    case OPTYPE_DUAL1:
        dReg   = (data & 0x03C0) >> 6;
        sMode  = (data & 0x0030) >> 4;
        sReg   =  data & 0x000F;
        break;

    case OPTYPE_DUAL2:
        dMode  = (data & 0x0C00) >> 10;
        dReg   = (data & 0x03C0) >> 6;
        sMode  = (data & 0x0030) >> 4;
        sReg   =  data & 0x000F;
        break;
    default:
        halt ("Bad optype");
    }

    if ((data & o->opMask) == OP_LDCR || (data & o->opMask) == OP_STCR)
    {
        mprintf(LVL_CPU, "CRU count %d, isbyte=%d force ?\n", dReg, isByte);
        if (dReg <= 8)
        {
            mprintf(LVL_CPU, "CRU count %d, force byte\n", dReg);
            isByte = 1;
        }
    }
            
    operand (sMode, sReg, &sArg, &sAddr, isByte);

    if (isByte)
        sData = memReadB (sAddr) << 8;
    else
        sData = memReadW (sAddr);

    if (o->hasDest)
    {
        operand (dMode, dReg, &dArg, &dAddr, isByte);

        if (isByte)
            dData = memReadB (dAddr) << 8;
        else
            dData = memReadW (dAddr);
    }

    if (unasmPreExecHook)
    {
        unasmPreExecHook (pc, data, o->opMask, o->type,
                          sMode, sReg, sArg,
                          dMode, dReg, dArg,
                          count, offset);
    }

    switch (data & o->opMask)
    {

    /*
     *  D U A L   O P E R A N D
     */

    case OP_SZC: dData &= ~sData;  doStore = 1; doCmpZ = 1;      break;
    case OP_S:
        u32 = (U32) dData - sData;
        dData = u32 & 0xFFFF;
        u32 >>= 16;

        /* 15-AUG-23 carry flag meaning is inverted for S, SB, DEC, DECT.  Where
         * is this documented ??????
         */
        // carrySet (u32 != 0);
        carrySet (u32 == 0);
        doStore = 1;
        doCmpZ = 1;
        break;
    case OP_C:   compare (sData, dData);                                break;
    case OP_MOV: dData = sData;           doStore = 1; doCmpZ = 1;      break;
    case OP_SOC: dData |= sData;   doStore = 1; doCmpZ = 1;      break;

    case OP_A:
        u32 = (U32) dData + sData;
        dData = u32 & 0xFFFF;
        u32 >>= 16;

        carrySet (u32 != 0);

        doStore = 1;
        doCmpZ = 1;
        break;

    case OP_COC:  compare (sData & dData, sData);             break;
    case OP_CZC:  compare (sData & ~dData, sData);            break;
    case OP_XOR:  dData ^= sData;  doStore = 1; doCmpZ = 1; break;
    case OP_XOP:  halt ("Unsupported");
    case OP_MPY:
        u32 = dData * sData;
        REGW(dReg, u32 >> 16);
        REGW(dReg+1, u32 & 0xFFFF);
        break;

    case OP_DIV:
        if (sData <= dData)
        {
            tms9900.st |= FLAG_OV;
        }
        else
        {
            u32 = REGR(dReg) << 16 | REGR(dReg+1);
            REGW(dReg, u32 / sData);
            REGW(dReg+1, u32 % sData);
        }

        break;

    /*
     *  I M M E D I A T E S
     */

    case OP_LI:
        memWriteW(sAddr, cpuFetch());
        break;
    case OP_AI:
        u32 = sData + cpuFetch();
        carrySet (u32 > 0x10000);
        u32 &= 0xffff;
        memWriteW(sAddr, u32);
        doCmpZ = 1;
        break;
    case OP_ANDI:
        memWriteW(sAddr, sData & cpuFetch());
        doCmpZ = 1;
        break;
    case OP_ORI:
        memWriteW(sAddr, sData | cpuFetch());
        doCmpZ = 1;
        break;
    case OP_CI:
        compare (sData, cpuFetch());
        break;
    case OP_STST:
        memWriteW(sAddr, tms9900.st);
        break;
    case OP_STWP:
        memWriteW(sAddr, tms9900.wp);
        break;
    case OP_LWPI:
        tms9900.wp = cpuFetch();
        break;
    case OP_LIMI:
        tms9900.st = (tms9900.st & ~FLAG_MSK) | cpuFetch();
        break;

    /*
     *  J U M P
     */
    case OP_JMP: jumpAnd (0,        0,                  offset);    break;
    case OP_JLT: jumpAnd (0,        FLAG_AGT | FLAG_EQ, offset);    break;
    case OP_JGT: jumpAnd (FLAG_AGT, FLAG_EQ,                  offset);    break;
    case OP_JL:  jumpAnd (0,        FLAG_LGT | FLAG_EQ, offset);    break;
    case OP_JLE: jumpOr  (FLAG_EQ,  FLAG_LGT,           offset);    break;
    case OP_JH:  jumpAnd (FLAG_LGT, FLAG_EQ,                  offset);    break;
    case OP_JHE: jumpOr  (FLAG_LGT | FLAG_EQ, 0,            offset);    break;
    case OP_JNC: jumpAnd (0,        FLAG_C,             offset);    break;
    case OP_JOC: jumpAnd (FLAG_C,   0,                  offset);    break;
    case OP_JNO: jumpAnd (0,        FLAG_OV,            offset);    break;
    case OP_JNE: jumpAnd (0,        FLAG_EQ,            offset);    break;
    case OP_JEQ: jumpAnd (FLAG_EQ,  0,                  offset);    break;

    case OP_SBZ:        cruBitSet (REGR(12), offset, 0);        break;
    case OP_SBO:        cruBitSet (REGR(12), offset, 1);        break;

    case OP_TB:
        tms9900.st |= (cruBitGet (REGR(12), offset) ? FLAG_EQ : 0);
        break;

    /*
     *  C R U
     */
    case OP_LDCR:
        if (isByte)
        {
            sData >>= 8;
        }

        cruMultiBitSet (REGR(12), sData, dReg);
        mprintf(LVL_CPU, "LDCR R12=%x s=%x d=%x\n", REGR(12), sData, dReg);
        break;
    case OP_STCR:
        if (isByte)
        {
            memWriteB(sAddr, cruMultiBitGet (REGR(12), dReg));
        }
        else
        {
            memWriteW(sAddr, cruMultiBitGet (REGR(12), dReg));
        }
        break;

    case OP_SRA:
        i32 = sData << 16;
        i32 >>= count;

        carrySet ((i32 & 0x8000) != 0);

        sData = (i32 >> 16) & 0xffff;
        doStore = 1;
        doCmpZ = 1;
        break;

    case OP_SRC:
        u32 = sData | (sData << 16);
        mprintf (LVL_CPU, "u32=%x\n", u32);
        u32 >>= count;
        mprintf (LVL_CPU, "u32=%x\n", u32);

        carrySet ((u32 & 0x8000) != 0);

        sData = u32 & 0xffff;
        doStore = 1;
        doCmpZ = 1;
        break;

    case OP_SRL:
        u32 = sData << 16;
        u32 >>= count;

        carrySet ((u32 & 0x8000) != 0);

        sData = (u32 >> 16) & 0xffff;
        doStore = 1;
        doCmpZ = 1;
        break;

    case OP_SLA:
        u32 = sData;
        u32 <<= count;

        /*
         *  Set carry flag on MSB set
         */
        carrySet ((u32 & 0x10000) != 0);

        sData = u32 & 0xFFFF;
        doStore = 1;
        doCmpZ = 1;
        break;

    case OP_BLWP:   blwp (sAddr);                                       break;
    case OP_RTWP:   rtwp ();                                            break;
    case OP_B:      tms9900.pc = sAddr;                                 break;
    case OP_X:
        mprintf (LVL_CPU, "X : recurse\n");
        cpuExecute (sData);
        break;
    case OP_CLR:    sData = 0;         doStore=1;                                 break;
    case OP_NEG:    sData = -sData;    doStore=1;   doCmpZ = 1;         break;
    case OP_INV:    sData = ~sData;    doStore=1;   doCmpZ = 1;         break;
    case OP_INC:
        carrySet (sData == 0xFFFF);
        sData += 1;
        doStore=1;
        doCmpZ = 1;
        break;
    case OP_INCT:
        carrySet ((sData & 0xFF00) == 0xFF00 || (sData & 0xFF00) == 0xFE00);
        sData += 2;
        doStore=1;
        doCmpZ = 1;
        break;
    case OP_DEC:
        carrySet (sData != 0);
        sData -= 1;
        doStore=1;
        doCmpZ = 1;
        break;
    case OP_DECT:
        carrySet (sData != 0 && sData != 1);
        sData -= 2;
        doStore=1;
        doCmpZ = 1;
        break;
    case OP_BL:
        REGW(11, tms9900.pc);
        tms9900.pc = sAddr;
        break;
    case OP_SWPB:   sData = SWAP(sData); doStore=1;                     break;
    case OP_SETO:   sData = 0xFFFF;      doStore=1;                     break;
    case OP_ABS:    sData = ((signed) sData < 0) ? -sData : sData; doStore=1;     break;

    default:
        mprintf (LVL_CPU, "%04X:%04X\n", pc, data & o->opMask);
        halt ("Unknown opcode");
    }

    if (!o->hasDest)
    {
        dAddr = sAddr;
        dData = sData;
        dReg = sReg;
    }

    if (doStore)
    {
        if (isByte)
            memWriteB (dAddr, (WORD) (dData >> 8));
        else
            memWriteW (dAddr, dData);
    }

    if (doCmpZ)
        compare (dData, 0);

    if (unasmPostExecHook)
    {
        unasmPostExecHook (pc, o->type, isByte, doStore, sMode, sAddr, dMode, dReg, dAddr, dData, REGR(dReg));
    }

    int mask = tms9900.st & FLAG_MSK;
    int level = interruptLevel (mask);

    if (level >= 0)
    {
        mprintf (LVL_CPU, "interrupt level=%d st=%x\n", level, tms9900.st);
        blwp (4 * level);

        /*  The ISR mask is automatically lowered to 1 less than the interrupt
         *  being serviced to ensure only higher priority interrupts can
         *  interrupt this ISR.
         */
        if (level > 0)
            mask = level - 1;
        else
            mask = 0;

        tms9900.st = (tms9900.st & ~FLAG_MSK) | mask;
    }
}

void cpuShowStatus(void)
{
    WORD i;

    printf ("CPU\n");
    printf ("===\n");
    printf ("st=%04X\nwp=%04X\npc=%04X\n", tms9900.st, tms9900.wp, tms9900.pc);

    for (i = 0; i < 16; i++)
    {
        printf ("R%02d: %04X ", i, REGR(i));
        if ((i + 1) % 4 == 0)
            printf ("\n");
    }
}

void cpuShowStWord(void)
{
    int st = tms9900.st;

    mprintf (LVL_CPU, "st=%04X (%s%s%s%s%s%s%s int=%d)\n",
             st,
             st & 0x8000 ? "L" : "",
             st & 0x4000 ? "A" : "",
             st & 0x2000 ? "=" : "",
             st & 0x1000 ? "C" : "",
             st & 0x0800 ? "O" : "",
             st & 0x0400 ? "P" : "",
             st & 0x0200 ? "X" : "", st & 15);
}

void cpuBoot (void)
{
    tms9900.st = 0;
    blwp (0x0);
}

