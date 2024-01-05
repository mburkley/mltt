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
 *  Implements TMS9900 CPU
 */

// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <stdarg.h>

#define __STANDALONE 1
#include "types.h"
// #include "vdp.h"
#include "cpu.h"
// #include "grom.h"
// #include "break.h"
// #include "watch.h"
// #include "cond.h"
#include "interrupt.h"
#include "cru.h"
#include "unasm.h"
#include "mem.h"
#include "trace.h"

struct
{
    uint16_t pc;
    uint16_t wp;
    uint16_t st;
}
tms9900;

typedef struct
{
    uint16_t index;
    uint16_t type;
    uint16_t opMask;
}
OpGroup;

static OpGroup opGroup[64] =
{
    { 0x0000, OPTYPE_IMMED,   0xFFE0 }, // LI, AI, ANDI, ORI, CI, STWP,
                                        // STST, LWPI, LIMI, RTWP
    { 0x0400, OPTYPE_SINGLE,  0xFFC0 }, // BLWP, B, X, CLR, NEG, INV,
                                        // INC(T), DEC(T), BL, SWPB, SETO, ABS
    { 0x0800, OPTYPE_SHIFT,   0xFF00 }, // SRA, SRL, SLA, SRC
    { 0x0C00, 0,              0xFFFF },
    { 0x1000, OPTYPE_JUMP,    0xFF00 }, // JMP, JLT, JLE, JEQ
    { 0x1400, OPTYPE_JUMP,    0xFF00 }, // JHE, JGT, JNE, JNC
    { 0x1800, OPTYPE_JUMP,    0xFF00 }, // JOC, JNO, JL, JH
    { 0x1C00, OPTYPE_JUMP,    0xFF00 }, // JOP, SBO, SBZ, TZ
    { 0x2000, OPTYPE_DUAL1,   0xFC00 }, // COC
    { 0x2400, OPTYPE_DUAL1,   0xFC00 }, // CZC
    { 0x2800, OPTYPE_DUAL1,   0xFC00 }, // XOR
    { 0x2C00, OPTYPE_DUAL1,   0xFC00 }, // XOP
    { 0x3000, OPTYPE_DUAL1,   0xFC00 }, // LDCR
    { 0x3400, OPTYPE_DUAL1,   0xFC00 }, // STCR
    { 0x3800, OPTYPE_DUAL1,   0xFC00 }, // MPY
    { 0x3C00, OPTYPE_DUAL1,   0xFC00 }, // DIV
    { 0x4000, OPTYPE_DUAL2,   0xF000 }, // SZC
    { 0x4400, OPTYPE_DUAL2,   0xF000 },
    { 0x4800, OPTYPE_DUAL2,   0xF000 },
    { 0x4C00, OPTYPE_DUAL2,   0xF000 },
    { 0x5000, OPTYPE_DUAL2,   0xF000 }, // SZCB
    { 0x5400, OPTYPE_DUAL2,   0xF000 },
    { 0x5800, OPTYPE_DUAL2,   0xF000 },
    { 0x5C00, OPTYPE_DUAL2,   0xF000 },
    { 0x6000, OPTYPE_DUAL2,   0xF000 }, // S
    { 0x6400, OPTYPE_DUAL2,   0xF000 },
    { 0x6800, OPTYPE_DUAL2,   0xF000 },
    { 0x6C00, OPTYPE_DUAL2,   0xF000 },
    { 0x7000, OPTYPE_DUAL2,   0xF000 }, // SB
    { 0x7400, OPTYPE_DUAL2,   0xF000 },
    { 0x7800, OPTYPE_DUAL2,   0xF000 },
    { 0x7C00, OPTYPE_DUAL2,   0xF000 },
    { 0x8000, OPTYPE_DUAL2,   0xF000 }, // C
    { 0x8400, OPTYPE_DUAL2,   0xF000 },
    { 0x8800, OPTYPE_DUAL2,   0xF000 },
    { 0x8C00, OPTYPE_DUAL2,   0xF000 },
    { 0x9000, OPTYPE_DUAL2,   0xF000 }, // CB
    { 0x9400, OPTYPE_DUAL2,   0xF000 },
    { 0x9800, OPTYPE_DUAL2,   0xF000 },
    { 0x9C00, OPTYPE_DUAL2,   0xF000 },
    { 0xA000, OPTYPE_DUAL2,   0xF000 }, // A
    { 0xA400, OPTYPE_DUAL2,   0xF000 },
    { 0xA800, OPTYPE_DUAL2,   0xF000 },
    { 0xAC00, OPTYPE_DUAL2,   0xF000 },
    { 0xB000, OPTYPE_DUAL2,   0xF000 }, // AB
    { 0xB400, OPTYPE_DUAL2,   0xF000 },
    { 0xB800, OPTYPE_DUAL2,   0xF000 },
    { 0xBC00, OPTYPE_DUAL2,   0xF000 },
    { 0xC000, OPTYPE_DUAL2,   0xF000 }, // MOV
    { 0xC400, OPTYPE_DUAL2,   0xF000 },
    { 0xC800, OPTYPE_DUAL2,   0xF000 },
    { 0xCC00, OPTYPE_DUAL2,   0xF000 },
    { 0xD000, OPTYPE_DUAL2,   0xF000 }, // MOVB
    { 0xD400, OPTYPE_DUAL2,   0xF000 },
    { 0xD800, OPTYPE_DUAL2,   0xF000 },
    { 0xDC00, OPTYPE_DUAL2,   0xF000 },
    { 0xE000, OPTYPE_DUAL2,   0xF000 }, // SOCB
    { 0xE400, OPTYPE_DUAL2,   0xF000 },
    { 0xE800, OPTYPE_DUAL2,   0xF000 },
    { 0xEC00, OPTYPE_DUAL2,   0xF000 },
    { 0xF000, OPTYPE_DUAL2,   0xF000 }, // SOCB
    { 0xF400, OPTYPE_DUAL2,   0xF000 },
    { 0xF800, OPTYPE_DUAL2,   0xF000 },
    { 0xFC00, OPTYPE_DUAL2,   0xF000 }
};

#define REGR(r) memReadW(tms9900.wp+((r)<<1))
#define REGW(r,d) memWriteW(tms9900.wp+((r)<<1),d)

uint16_t cpuFetch (void)
{
    uint16_t ret;

    ret = memReadW(tms9900.pc);
    tms9900.pc += 2;
    return ret;
}

uint16_t cpuGetPC (void)
{
    return tms9900.pc;
}

uint16_t cpuGetWP (void)
{
    return tms9900.wp;
}

uint16_t cpuGetST (void)
{
    return tms9900.st;
}

uint16_t cpuGetIntMask (void)
{
    return tms9900.st & FLAG_MSK;
}

static void blwp (uint16_t addr)
{
    uint16_t owp = tms9900.wp;
    uint16_t opc = tms9900.pc;

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
static void jumpAnd (uint16_t setMask, uint16_t clrMask, uint16_t offset)
{
    if ((tms9900.st & setMask) == setMask &&
        (~tms9900.st & clrMask) == clrMask)
    {
        unasmPostText("st=%04X[s=%04X&&c=%04X], jump", tms9900.st, setMask,
        clrMask);
        tms9900.pc += offset << 1;
    }
}

/*  Jump if any bits in the set mask are set or any bits in the clear mask are
 *  clear */
static void jumpOr (uint16_t setMask, uint16_t clrMask, uint16_t offset)
{
    if ((tms9900.st & setMask) != 0 ||
        (~tms9900.st & clrMask) != 0)
    {
        unasmPostText("st=%04X[s=%04X||c=%04X], jump", tms9900.st, setMask,
        clrMask);
        tms9900.pc += offset << 1;
    }
}

static void statusCarry (bool condition)
{
    if (condition)
        tms9900.st |= FLAG_C;
    else
        tms9900.st &= ~FLAG_C;
}

static void statusOverflow (bool condition)
{
    if (condition)
        tms9900.st |= FLAG_OV;
    else
        tms9900.st &= ~FLAG_OV;
}

static void statusEqual (bool condition)
{
    if (condition)
        tms9900.st |= FLAG_EQ;
    else
        tms9900.st &= ~FLAG_EQ;
}

static void statusLogicalGreater (bool condition)
{
    if (condition)
        tms9900.st |= FLAG_LGT;
    else
        tms9900.st &= ~FLAG_LGT;
}

static void statusArithmeticGreater (bool condition)
{
    if (condition)
        tms9900.st |= FLAG_AGT;
    else
        tms9900.st &= ~FLAG_AGT;
}

static void statusParity (uint8_t value)
{
    bool oddParity = false;

    for (int i = 0; i < 8; i++)
        if (value & (1<<i))
            oddParity = !oddParity;

    // printf("OP %02x = %s\n", value, oddParity ? "ODD" : "EVEN");

    if (oddParity)
        tms9900.st |= FLAG_OP;
    else
        tms9900.st &= ~FLAG_OP;
}

static char *outputStatus (void)
{
    static char text[10];
    char *tp = text;
    int st = tms9900.st;

    *tp++ = '[';
    if (st & 0x8000) *tp++ = 'G';
    if (st & 0x4000) *tp++ = 'A';
    if (st & 0x2000) *tp++ = '=';
    if (st & 0x1000) *tp++ = 'C';
    if (st & 0x0800) *tp++ = 'O';
    if (st & 0x0400) *tp++ = 'P';
    if (st & 0x0200) *tp++ = 'X';
    *tp++=']';
    *tp++ = 0;

    return text;
}
static void compareWord (uint16_t sData, uint16_t dData)
{
    statusEqual (sData == dData);
    statusLogicalGreater (sData > dData);
    // unasmPostText("[AGT:%x>%x]", (int16_t)sData, (int16_t)dData);
    statusArithmeticGreater ((int16_t) sData > (int16_t) dData);
    unasmPostText (outputStatus());
}

static void compareByte (uint16_t sData, uint16_t dData)
{
    statusEqual (sData == dData);
    statusLogicalGreater (sData > dData);
    statusArithmeticGreater ((int8_t) sData > (int8_t) dData);
    unasmPostText (outputStatus());
}

static uint16_t operandDecode (uint16_t mode, uint16_t reg, bool isByte)
{
    uint16_t addr;

    switch (mode)
    {
    case AMODE_NORMAL:
        addr = tms9900.wp+(reg<<1);
        break;

    case AMODE_INDIR:
        addr = REGR(reg);
        break;

    case AMODE_SYM:
        addr = (uint16_t) (cpuFetch() + (reg == 0 ? 0 : REGR(reg)));
        break;

    case AMODE_INDIRINC:
        addr = REGR(reg);
        REGW (reg, REGR(reg) + (isByte ? 1 : 2));
        break;

    default:
        halt ("Bad operand mode");
    }

    return addr;
}

static uint16_t operandFetch (uint16_t mode, uint16_t reg, uint16_t addr, bool isByte, bool doFetch)
{
    uint16_t data = 0;

    if (isByte)
    {
        if (mode)
            unasmPostText("B:[%04X]", addr);
        else
            unasmPostText("R%d", reg);

        if (doFetch)
        {
            data = memReadB (addr);
            unasmPostText("=%02X", data);
        }
    }
    else
    {
        if (mode)
            unasmPostText("W:[%04X]", addr);
        else
            unasmPostText("R%d", reg);

        if (doFetch)
        {
            data = memReadW (addr);
            unasmPostText("=%04X", data);
        }
    }

    return data;
}

/*
 *  I M M E D I A T E S
 */

static void cpuExecuteImmediate (uint16_t opcode, uint16_t reg)
{
    uint16_t immed;
    uint32_t data;

    switch (opcode)
    {
    case OP_LI:
        immed = cpuFetch();
        REGW(reg,immed);
        break;

    case OP_AI:
        data = REGR(reg);
        immed = cpuFetch();
        /*  Overflow if MSB(data)=MSB(Imm) && MSB(result) != MSB (data) */
        statusOverflow ((data & 0x8000) == (immed & 0x8000) &&
                        ((data+immed) & 0x8000) != (data & 0x8000));
        unasmPostText ("R%d=%04X+%04X=%04X", reg, data, immed, data+immed);
        data += immed;
        statusCarry (data >= 0x10000);
        data &= 0xffff;
        REGW(reg,data);
        compareWord (data, 0);
        break;

    case OP_ANDI:
        data = REGR(reg);
        immed = cpuFetch();
        unasmPostText ("R%d=%04X&%04X=%04X", reg, data, immed, data&immed);
        data &= immed;
        REGW(reg,data);
        compareWord (data, 0);
        break;

    case OP_ORI:
        data = REGR(reg);
        immed = cpuFetch();
        unasmPostText ("R%d=%04X|%04X=%04X", reg, data, immed, data|immed);
        data |= immed;
        REGW(reg,data);
        compareWord (data, 0);
        break;

    case OP_CI:
        data = REGR(reg);
        unasmPostText ("R%d=%04X", reg, data);
        compareWord (data, cpuFetch());
        break;

    case OP_STST:
        immed = tms9900.st;
        unasmPostText ("R%d=%04X", reg, immed);
        REGW(reg,immed);
        break;

    case OP_STWP:
        immed = tms9900.wp;
        unasmPostText ("R%d=%04X", reg, immed);
        REGW(reg,immed);
        break;

    case OP_LWPI:
        immed = cpuFetch();
        tms9900.wp = immed;
        break;

    case OP_LIMI:
        tms9900.st = (tms9900.st & ~FLAG_MSK) | cpuFetch();
        break;

    case OP_RTWP:
        rtwp ();
        unasmPostText ("pc=%04X", tms9900.pc);
        break;

    default:
        halt ("Bad immediate opcode");
    }
}

static void cpuExecuteSingle (uint16_t opcode, uint16_t mode, uint16_t reg)
{
    uint16_t addr;
    uint16_t param;

    addr = operandDecode (mode, reg, false);

    if (mode)
        unasmPostText("W:[%04X]", addr);
    else
        unasmPostText("R%d", reg);

    switch (opcode)
    {
    case OP_BLWP:
        unasmPostText ("=%04X", addr);
        blwp (addr);
        break;

    case OP_B:
        unasmPostText ("=%04X", addr);
        tms9900.pc = addr;
        break;

    case OP_X:
        param = memReadW (addr);
        mprintf (LVL_CPU, "X : recurse\n");
        cpuExecute (param);
        break;

    case OP_CLR:
        memWriteW (addr, 0);
        break;

    case OP_NEG:
        param = memReadW (addr);
        statusCarry (param == 0x8000);
        statusOverflow (param & 0x8000);
        param = -param;
        unasmPostText ("=%04X", param);
        memWriteW (addr, param);
        compareWord (param, 0);
        break;

    case OP_INV:
        param = ~memReadW (addr);
        unasmPostText ("=%04X", param);
        memWriteW (addr, param);
        compareWord (param, 0);
        break;

    case OP_INC:
        param = memReadW (addr);
        statusOverflow ((param & 0x8000) == 0 &&
                        ((param + 1) & 0x8000) == 0x8000);
        statusCarry (param == 0xFFFF);
        param += 1;
        unasmPostText ("=%04X", param);
        memWriteW (addr, param);
        compareWord (param, 0);
        break;

    case OP_INCT:
        param = memReadW (addr);
        statusOverflow ((param & 0x8000) == 0 &&
                        ((param + 2) & 0x8000) == 0x8000);
        statusCarry ((param & 0xFFFE) == 0xFFFE);
        param += 2;
        if(addr&1)
        {
            param&=0xFF;
            unasmPostText ("=%02X", param);
            memWriteB(addr,param);
            compareByte (param, 0);
        }
        else
        {
            unasmPostText ("=%04X", param);
            memWriteW (addr, param);
            compareWord (param, 0);
        }
        break;

    case OP_DEC:
        param = memReadW (addr);
        statusCarry (param != 0);
        statusOverflow ((param & 0x8000) == 0x8000 &&
                        ((param - 1) & 0x8000) == 0);
        param -= 1;
        unasmPostText ("=%04X", param);
        memWriteW (addr, param);
        compareWord (param, 0);
        break;

    case OP_DECT:
        param = memReadW (addr);
        statusCarry (param != 0 && param != 1);
        statusOverflow ((param & 0x8000) == 0x8000 &&
                        ((param - 2) & 0x8000) == 0);
        param -= 2;
        /*  Not sure if this is strictly necessary, but in the ROM code there
         *  are several places where DECT is called on an odd address.  Does
         *  this mean only the low byte should be decremented?  Assume so for
         *  now.
         */
        if(addr&1)
        {
            param&=0xFF;
            unasmPostText ("=%02X", param);
            memWriteB(addr,param);
            compareByte (param, 0);
        }
        else
        {
            unasmPostText ("=%04X", param);
            memWriteW (addr, param);
            compareWord (param, 0);
        }
        break;

    case OP_BL:
        REGW(11, tms9900.pc);
        tms9900.pc = addr;
        break;

    case OP_SWPB:
        param = memReadW (addr);
        param = SWAP(param);
        unasmPostText ("=%04X", param);
        memWriteW (addr, param);
        compareWord (param, 0);
        break;

    case OP_SETO:
        memWriteW (addr, 0xFFFF);
        break;

    case OP_ABS:
        param = memReadW (addr);
        statusCarry (param == 0x8000);
        statusOverflow (param & 0x8000);
        /*  AGT for ABS is unusual in that it takes the sign of the source into
         *  account and doesn't just do a comparison of the result to zero */
        statusArithmeticGreater ((int8_t) param > 0);
        param = ((int16_t) param < 0) ? -param : param;
        unasmPostText ("=%04X", param);
        memWriteW (addr, param);
        statusEqual (param == 0);
        statusLogicalGreater (param != 0);
        unasmPostText (outputStatus());
        break;

    default:
        halt ("Bad single opcode");
    }
}

static void cpuExecuteShift (uint16_t opcode, uint16_t reg, uint16_t count)
{
    uint32_t u32;
    int32_t i32;

    if (count == 0)
        count = REGR(0) & 0x000F;

    if (count == 0)
        count = 16;

    switch (opcode)
    {
    case OP_SRA:
        i32 = REGR (reg) << 16;
        unasmPostText ("%04X=>", i32>>16);
        i32 >>= count;

        /* Set carry flag if last bit shifted is set */
        statusCarry ((i32 & 0x8000) != 0);

        u32 = (i32 >> 16) & 0xffff;
        REGW (reg, u32);
        compareWord (u32, 0);
        break;

    case OP_SRC:
        u32 = REGR (reg);
        unasmPostText ("%04X=>", u32);
        u32 |= (u32 << 16);
        mprintf (LVL_CPU, "u32=%x\n", u32);
        u32 >>= count;
        mprintf (LVL_CPU, "u32=%x\n", u32);

        /* Set carry flag if last bit shifted is set */
        statusCarry ((u32 & 0x8000) != 0);

        u32 &= 0xffff;
        REGW (reg, u32);
        compareWord (u32, 0);
        break;

    case OP_SRL:
        u32 = REGR (reg) << 16;
        unasmPostText ("%04X=>", u32>>16);
        u32 >>= count;

        /* Set carry flag if last bit shifted is set */
        statusCarry ((u32 & 0x8000) != 0);

        u32 >>= 16;
        REGW (reg, u32);
        compareWord (u32, 0);
        break;

    case OP_SLA:
        i32 = REGR (reg);
        unasmPostText ("%04X=>", i32);
        u32 = i32 << count;

        /* Set carry flag if last bit shifted is set */
        statusCarry ((u32 & 0x10000) != 0);

        /* Set if MSB changes */
        statusOverflow ((u32 & 0x8000) != (i32 & 0x8000));

        u32 &= 0xFFFF;
        REGW (reg, u32);
        compareWord (u32, 0);
        break;

    default:
        halt ("Bad shift opcode");
    }

    unasmPostText ("%04X", u32);
}

/*
 *  J U M P
 */
static void cpuExecuteJump (uint16_t opcode, int16_t offset)
{
    switch (opcode)
    {
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

    case OP_SBZ:        cruBitOutput (REGR(12), offset, 0);        break;
    case OP_SBO:        cruBitOutput (REGR(12), offset, 1);        break;

    case OP_TB:
        tms9900.st &= ~FLAG_EQ;
        tms9900.st |= (cruBitGet (REGR(12), offset) ? FLAG_EQ : 0);
        break;

    default:
        halt ("Bad jump opcode");
    }
}

static void cpuExecuteDual1 (uint16_t opcode, uint16_t dReg, uint16_t sMode, uint16_t sReg)
{
    uint16_t sAddr;
    uint16_t sData;
    uint16_t dData;
    uint32_t u32;
    
    sAddr = operandDecode (sMode, sReg, false);
    sData = operandFetch (sMode, sReg, sAddr, false, true);

    switch (opcode)
    {
    case OP_COC:
        dData = REGR (dReg);
        unasmPostText ("&(R%d=%04X)=%04X", dReg, dData, sData & dData);
        compareWord (sData & dData, sData);
        break;

    case OP_CZC:
        dData = REGR (dReg);
        unasmPostText ("&~(R%d=%04X)=%04X", dReg, dData, sData & ~dData);
        compareWord (sData & ~dData, sData);
        break;

    case OP_XOR:
        dData = REGR (dReg);
        unasmPostText ("&~(R%d=%04X)=%04X", dReg, dData, sData ^ dData);
        dData ^= sData;
        REGW (dReg, dData);
        compareWord (dData, 0);
        break;

    case OP_XOP:
        halt ("Unsupported");
        break;

    case OP_MPY:
        dData = REGR (dReg);
        unasmPostText ("*(R%d=%04X)=%04X", dReg, dData, sData * dData);
        u32 = dData * sData;
        REGW(dReg, u32 >> 16);
        REGW(dReg+1, u32 & 0xFFFF);
        break;

    case OP_DIV:
        dData = REGR (dReg);
        if (sData <= dData)
        {
            unasmPostText ("<(%04X<%04X)->OVF", sData, dData);
            statusOverflow (true);
        }
        else
        {
            statusOverflow (false);
            u32 = REGR(dReg) << 16 | REGR(dReg+1);
            unasmPostText (",(%X/%X)=>%04X,%04X", u32, sData, u32 / sData, u32 % sData);
            REGW(dReg, u32 / sData);
            REGW(dReg+1, u32 % sData);
        }

        break;

    /*
     *  C R U
     */
    case OP_LDCR:
        if (dReg <= 8)
        {
            sData = memReadB (sAddr);
            statusParity (sData);
        }

        cruMultiBitSet (REGR(12), sData, dReg);
        mprintf(LVL_CPU, "LDCR R12=%x s=%x d=%x\n", REGR(12), sData, dReg);
        break;

    case OP_STCR:
        if (dReg <= 8)
        {
            sData = cruMultiBitGet (REGR(12), dReg);
            memWriteB(sAddr, sData);
            statusParity (sData);
        }
        else
            memWriteW(sAddr, cruMultiBitGet (REGR(12), dReg));
        break;

    default:
        halt ("Bad dual1 opcode");
    }
}

/*
 *  D U A L   O P E R A N D
 */
static void cpuExecuteDual2 (uint16_t opcode, uint16_t dMode, uint16_t dReg,
                             uint16_t sMode, uint16_t sReg, bool isByte)
{
    bool doStore = true;
    uint16_t sAddr;
    uint16_t sData;
    uint16_t dAddr;
    uint16_t dData;
    uint32_t u32;

    sAddr = operandDecode (sMode, sReg, isByte);
    sData = operandFetch (sMode, sReg, sAddr, isByte, true);

    unasmPostText (",");
    dAddr = operandDecode (dMode, dReg, isByte);
    /*  Don't fetch the contents of the destination if op is a MOV */
    dData = operandFetch (dMode, dReg, dAddr, isByte,
                          (opcode != OP_MOV && opcode != OP_MOVB));

    switch (opcode)
    {
    case OP_SZC:
        dData &= ~sData;
        unasmPostText (":&~:%04X", dData);
        compareWord (dData, 0);
        break;

    case OP_SZCB:
        dData &= ~sData;
        unasmPostText (":&~:%02X", dData);
        compareByte (dData, 0);
        statusParity (dData);
        break;

    case OP_S:
        u32 = (uint32_t) dData - sData;
        statusOverflow ((sData & 0x8000) != (dData & 0x8000) &&
                        (u32 & 0x8000) != (dData & 0x8000));
        dData = u32 & 0xFFFF;
        u32 >>= 16;
        unasmPostText (":-:%04X", dData);

        /* 15-AUG-23 carry flag meaning is inverted for S, SB, DEC, DECT.  Where
         * is this documented ??????
         */
        // statusCarry (u32 != 0);
        statusCarry (u32 == 0);
        compareWord (dData, 0);
        break;

    case OP_SB:
        u32 = (uint32_t) dData - sData;
        statusOverflow ((sData & 0x8000) != (dData & 0x8000) &&
                        (u32 & 0x8000) != (dData & 0x8000));
        dData = u32 & 0xFF;
        u32 >>= 8;
        unasmPostText (":-:%02X", dData);

        /* 15-AUG-23 carry flag meaning is inverted for S, SB, DEC, DECT.  Where
         * is this documented ??????
         */
        // statusCarry (u32 != 0);
        statusCarry (u32 == 0);
        compareByte (dData, 0);
        statusParity (dData);
        break;

    case OP_C:
        compareWord (sData, dData);
        unasmPostText (":==:");
        doStore = false;
        break;

    case OP_CB:
        compareByte (sData, dData);
        statusParity (sData);
        unasmPostText (":==:");
        doStore = false;
        break;

    case OP_MOV:
        dData = sData;
        compareWord (dData, 0);
        break;

    case OP_MOVB:
        dData = sData;
        statusParity (sData);
        compareByte (dData, 0);
        break;

    case OP_SOC:
        dData |= sData;
        unasmPostText (":|:%04X", dData);
        compareWord (dData, 0);
        break;

    case OP_SOCB:
        dData |= sData;
        unasmPostText (":|:%02X", dData);
        compareByte (dData, 0);
        statusParity (dData);
        break;

    case OP_A:
        u32 = (uint32_t) dData + sData;
        statusOverflow ((sData & 0x8000) == (dData & 0x8000) &&
                        (u32 & 0x8000) != (dData & 0x8000));
        dData = u32 & 0xFFFF;
        unasmPostText (":+:%04X", dData);
        u32 >>= 16;

        statusCarry (u32 != 0);
        compareWord (dData, 0);
        break;

    case OP_AB:
        u32 = (uint32_t) dData + sData;
        statusOverflow ((sData & 0x8000) == (dData & 0x8000) &&
                        (u32 & 0x8000) != (dData & 0x8000));
        dData = u32 & 0xFF;
        unasmPostText (":+:%02X", dData);
        u32 >>= 8;

        statusCarry (u32 != 0);
        compareByte (dData, 0);
        statusParity (dData);
        break;

    default:
        halt ("Bad dual2 opcode");
    }

    if (doStore)
    {
        if (isByte)
            memWriteB (dAddr, dData);
        else
            memWriteW (dAddr, dData);
    }
}

uint16_t cpuDecode (uint16_t data, uint16_t *type)
{
    OpGroup *o = &opGroup[data >> 10];
    *type = o->type;
    return (data & o->opMask);
}

void cpuExecute (uint16_t data)
{
    int sReg = 0;
    int sMode = 0;
    int dReg = 0;
    int dMode = 0;
    int8_t offset;
    bool isByte = false;
    uint16_t type;

    uint16_t opcode = cpuDecode (data, &type);

    unasmPreExec (tms9900.pc, data, type, opcode);

    switch (type)
    {
    case OPTYPE_IMMED:
        sReg   =  data & 0x000F;
        cpuExecuteImmediate (opcode, sReg);
        break;

    case OPTYPE_SINGLE:
        sMode = (data & 0x0030) >> 4;
        sReg     =  data & 0x000F;
        cpuExecuteSingle (opcode, sMode, sReg);
        break;

    case OPTYPE_SHIFT:
        dReg = (data & 0x00F0) >> 4;
        sReg =  data & 0x000F;
        cpuExecuteShift (opcode, sReg, dReg);
        break;

    case OPTYPE_JUMP:
        offset = data & 0x00FF;
        cpuExecuteJump (opcode, offset);
        break;

    case OPTYPE_DUAL1:
        dReg     = (data & 0x03C0) >> 6;
        sMode = (data & 0x0030) >> 4;
        sReg     =  data & 0x000F;
        cpuExecuteDual1 (opcode, dReg, sMode, sReg);
        break;

    case OPTYPE_DUAL2:
        dMode = (data & 0x0C00) >> 10;
        dReg     = (data & 0x03C0) >> 6;
        sMode = (data & 0x0030) >> 4;
        sReg     =  data & 0x000F;
        isByte = (data & 0x1000) >> 12;
        cpuExecuteDual2 (opcode, dMode, dReg, sMode, sReg, isByte);
        break;

    default:
        halt ("Bad optype");
    }

    unasmPostPrint();

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
    uint16_t i;

    mprintf (0, "CPU\n");
    mprintf (0, "===\n");
    mprintf (0, "st=%04X\nwp=%04X\npc=%04X\n", tms9900.st, tms9900.wp, tms9900.pc);

    for (i = 0; i < 16; i++)
    {
        mprintf (0, "R%02d: %04X ", i, REGR(i));
        if ((i + 1) % 4 == 0)
            mprintf (0, "\n");
    }
}

void cpuShowStWord(void)
{
    int st = tms9900.st;

    mprintf (0, "st=%04X %s int=%d)\n",
             st, outputStatus(), st & 15);
}

void cpuBoot (void)
{
    tms9900.st = 0;
    blwp (0x0);
}

