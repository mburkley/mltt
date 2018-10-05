/*
 *  TODO :
 *
 *  - Make this CPU only.
 *  - Move interaction into debug module.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <conio.h>

#include "vdp.h"
#include "cpu.h"
#include "grom.h"
#include "break.h"
#include "watch.h"
#include "cond.h"
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

    WORD *r[16];
}
tms9900;

typedef struct
{
    WORD index;
    WORD type;
    WORD opMask;
    BOOL store;
    BOOL hasDest;
    BOOL isByte;
    BOOL cmpZero;
}
OpGroup;

OpGroup opGroup[64] =
{
    { 0x00, OPTYPE_IMMED,   0xFFE0, 0, 0, 0, 1 },
    { 0x01, OPTYPE_SINGLE,  0xFFC0, 0, 0, 0, 1 },
    { 0x02, OPTYPE_SHIFT,   0xFF00, 0, 0, 0, 1 },
    { 0x03, 0,              0xFFFF, 0, 0, 0, 0 },
    { 0x04, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x05, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x06, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x07, OPTYPE_JUMP,    0xFF00, 0, 0, 0, 0 },
    { 0x08, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x09, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x0A, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x0B, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x0C, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x0D, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x0E, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x0F, OPTYPE_DUAL1,   0xFC00, 1, 1, 0, 1 },
    { 0x10, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x11, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x12, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x13, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x14, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x15, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x16, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x17, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x18, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x19, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x1A, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x1B, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x1C, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x1D, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x1E, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x1F, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x20, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x21, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x22, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x23, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x24, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x25, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x26, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x27, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x28, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x29, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x2A, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x2B, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x2C, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x2D, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x2E, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x2F, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x30, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x31, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x32, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x33, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x34, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x35, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x36, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x37, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x38, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x39, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x3A, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x3B, OPTYPE_DUAL2,   0xE000, 1, 1, 0, 1 },
    { 0x3C, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x3D, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x3E, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 },
    { 0x3F, OPTYPE_DUAL2,   0xE000, 1, 1, 1, 1 }
};

int outputLevel;

#define AREG(n)  (WORD)(tms9900.wp+(n<<1))
// #define REG(n)  memRead(AREG(n))

// #define AREG(n)  tms9900.r[n]
#define REG(n)  *tms9900.r[n]


int mprintf (int level, char *s, ...)
{
    va_list ap;

    va_start (ap, s);

    if ((1<<level) & outputLevel)
        vprintf (s, ap);

    va_end (ap);

    return 0;
}

void halt (char *s)
{
    vdpRefresh(1);

    printf ("HALT: %s\n", s);

    exit (1);
}

static WORD fetch (void)
{
    WORD ret;

    // if (tms9900.pc >= 0x2000)
    // {
    //     printf ("PC=%04X\n", tms9900.pc);
    //     halt ("Fetch from outside ROM area\n");
    // }

    ret = memRead(tms9900.pc);
    tms9900.pc += 2;
    return ret;
}

static void setwp (WORD wp)
{
    int i;

    tms9900.wp = wp;

    for (i = 0; i < 16; i++)
    {
        // tms9900.r[i] = &tms9900.ram.w[wp + i];
        tms9900.r[i] = memWordAddr (wp + (i << 1));
    }

    // printf ("[SETWP:R13,R14,R15=%04X,%04X,%04X]\n",
    //        *(tms9900.r[13]),
    //        *(tms9900.r[14]),
    //        *(tms9900.r[15]));
}

static void blwp (WORD addr)
{
    WORD owp = tms9900.wp;
    WORD opc = tms9900.pc;

    // printf ("blwp @%x\n", addr);

    setwp (memRead (addr));
    tms9900.pc = memRead ((WORD) (addr+2));

    REG (13) = owp;
    REG (14) = opc;
    REG (15) = tms9900.st;
}

static void rtwp (void)
{
    // printf ("rtwp\n");

    tms9900.pc = REG (14);
    tms9900.st = REG (15);
    // tms9900.wp = REG (13);

    setwp (REG (13));
}

static void jumpAnd (WORD setMask, WORD clrMask, WORD offset)
{
    if ((tms9900.st & setMask) == setMask &&
        (~tms9900.st & clrMask) == clrMask)
    {
        tms9900.pc += offset << 1;
    }
}

static void jumpOr (WORD setMask, WORD clrMask, WORD offset)
{
    if ((tms9900.st & setMask) == setMask ||
        (~tms9900.st & clrMask) == clrMask)
    {
        tms9900.pc += offset << 1;
    }
}

static void compare (WORD data1, WORD data2)
{
    /*
     *  Leave int. mask and Carry flag
     */

    tms9900.st &= FLAG_MSK;

    if (data1 == data2)
    {
        tms9900.st |= FLAG_EQ;
    }

    if (data1 > data2)
    {
        tms9900.st |= FLAG_LGT;
    }

    if ((signed short) data1 > (signed short) data2)
    {
        tms9900.st |= FLAG_AGT;
    }

    // mprintf (LVL_CPU, "CMP %04X,%04X %s%s%s%s ",
    //          data1, data2,
    //          tms9900.st & FLAG_C ? "C" : "-",
    //          tms9900.st & FLAG_EQ ? "E" : "-",
    //          tms9900.st & FLAG_LGT ? "L" : "-",
    //          tms9900.st & FLAG_AGT ? "A" : "-");
}

static void compareB (BYTE data1, BYTE data2)
{
    tms9900.st &= FLAG_MSK;

    if (data1 == data2)
    {
        tms9900.st |= FLAG_EQ;
    }

    if (data1 > data2)
    {
        tms9900.st |= FLAG_LGT;
    }

    if ((signed char) data1 > (signed char) data2)
    {
        tms9900.st |= FLAG_AGT;
    }

    // mprintf (LVL_CPU, "CMPB: %02X : %02X %s %s %s\n",
    //         data1, data2,
    //         tms9900.st & FLAG_EQ ? "EQ" : "",
    //         tms9900.st & FLAG_LGT ? "LGT" : "",
    //         tms9900.st & FLAG_AGT ? "AGT" : "");

}

static void operand (WORD mode, WORD reg, WORD *arg, WORD *addr, BOOL isByte)
{
    switch (mode)
    {
    //case AMODE_NORMAL:
    //    *addr = AREG(reg);
    //    break;

    case AMODE_INDIR:
        *addr = REG(reg);
        break;

    case AMODE_SYM:
        *arg = fetch();
        *addr = (WORD) (*arg + (reg == 0 ? 0 : REG (reg)));
        // printf ("[SYM]");
        break;

    case AMODE_INDIRINC:
        *addr = REG(reg);
        REG(reg) += (isByte ? 1 : 2);
        break;
    default:
        halt ("Bad operand mode");
    }
}

static void execute (WORD data)
{
    WORD  sReg = 0, dReg = 0;
    WORD sAddr = 0, dAddr = 0;
    WORD sData = 0, dData = 0;
    WORD dMode = 0, sMode = 0;
    WORD sArg = 0, dArg = 0;
    WORD pc = tms9900.pc;
    BOOL doStore = 0;
    BOOL doCmpZ = 0;
    I8 offset = 0;
    WORD count = 0;
    U32 u32 = 0;
    I32 i32 = 0;
    BOOL carry = 0;
    OpGroup *o = &opGroup[data >> 10];

    // printf ("T:%d ", o->type);

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
        // printf ("[dual:sm=%d,dm=%d]", sMode, dMode);
        break;
    default:
        halt ("Bad optype");
    }

    if (sMode)
    {
        operand (sMode, sReg, &sArg, &sAddr, o->isByte);

        if (o->isByte)
        {
            // printf ("[SM:BYTE:%04X]", sAddr);
            sData = memReadB (sAddr) << 8;
        }
        else
        {
            // printf ("[DM:WORD:%04X]", sAddr);
            sData = memRead (sAddr);
        }
    }
    else
        sData = REG(sReg);

    if (dMode)
    {
        operand (dMode, dReg, &dArg, &dAddr, o->isByte);

        if (o->isByte)
        {
            // printf ("[DM:BYTE]");
            dData = memReadB (dAddr) << 8;
        }
        else
        {
            // printf ("[DM:WORD]");
            dData = memRead (dAddr);
        }
    }
    else
        dData = REG(dReg);

    if (o->isByte)
    {
        sData &= 0xFF00;
        dData &= 0xFF00;
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
    case OP_S:   dData -= sData;   doStore = 1; doCmpZ = 1;      break;
    case OP_C:   compare (sData, dData);                                break;
    case OP_MOV: dData = sData;           doStore = 1; doCmpZ = 1;      break;
    case OP_SOC: dData |= sData;   doStore = 1; doCmpZ = 1;      break;

    case OP_A:
        u32 = (U32) dData + sData;
        dData = u32 & 0xFFFF;
        u32 >>= 16;

        if (u32)
            carry = 1;

        doStore = 1;
        doCmpZ = 1;

        break;


    /*
     *
     */

    case OP_COC:  compare (sData & dData, sData);             break;
    case OP_CZC:  compare (sData & ~dData, sData);            break;
    case OP_XOR:  dData ^= sData;  doStore = 1; doCmpZ = 1; break;
    case OP_XOP:  halt ("Unsupported");
    case OP_MPY:
        /*
         *  TODO: double-check this
         */
        u32 = dData * sData;
        REG(dReg)   = (WORD) (u32 >> 16);
        REG(dReg+1) = (WORD) (u32 & 0xFFFF);
        doCmpZ = 1;
        break;

    case OP_DIV:
        /*
         *  TODO: double-check this
         */
        u32 = REG(dReg) << 16 | REG(dReg+1);
        if (sData == 0)
        {
            tms9900.st |= FLAG_OV;
        }
        else
        {
            u32 /= sData;
        }

        REG(dReg)   = (WORD) (u32 >> 16);
        REG(dReg+1) = (WORD) (u32 & 0xFFFF);
        break;

    /*
     *  I M M E D I A T E S
     */

    case OP_LI:        REG(sReg) = fetch();                             break;
    case OP_AI:        REG(sReg) += fetch();    doCmpZ = 1;             break;
    case OP_ANDI:      REG(sReg) &= fetch();    doCmpZ = 1;             break;
    case OP_ORI:       REG(sReg) |= fetch();    doCmpZ = 1;             break;
    case OP_CI:        compare (sData, fetch());                        break;
    case OP_STST:      REG(sReg) = tms9900.st;                          break;
    case OP_STWP:      REG(sReg) = tms9900.wp;                          break;
    case OP_LWPI:      setwp (fetch());                                 break;

    case OP_LIMI:
        tms9900.st = (tms9900.st & ~FLAG_MSK) | fetch();
        break;

    /*
     *  J U M P
     */

    case OP_JMP: jumpAnd (0,        0,                  offset);    break;
    case OP_JLT: jumpAnd (0,        FLAG_AGT | FLAG_EQ, offset);    break;
    case OP_JGT: jumpAnd (FLAG_AGT, 0,                  offset);    break;
    case OP_JL:  jumpAnd (0,        FLAG_LGT | FLAG_EQ, offset);    break;
    case OP_JLE: jumpOr  (FLAG_EQ,  FLAG_LGT,           offset);    break;
    case OP_JH:  jumpAnd (FLAG_LGT, 0,                  offset);    break;
    case OP_JHE: jumpOr  (FLAG_LGT, FLAG_EQ,            offset);    break;
    case OP_JNC: jumpAnd (0,        FLAG_C,             offset);    break;
    case OP_JOC: jumpAnd (FLAG_C,   0,                  offset);    break;
    case OP_JNO: jumpAnd (0,        FLAG_OV,            offset);    break;
    case OP_JNE: jumpAnd (0,        FLAG_EQ,            offset);    break;
    case OP_JEQ: jumpAnd (FLAG_EQ,  0,                  offset);    break;

    case OP_SBZ:        cruBitSet (REG(12), offset, 0);        break;
    case OP_SBO:        cruBitSet (REG(12), offset, 1);        break;

    case OP_TB:
        tms9900.st |= (cruBitGet (REG(12), offset) ? FLAG_EQ : 0);
        break;

    /*
     *
     */

    case OP_LDCR: cruMultiBitSet (REG(12), REG(sReg), count);           break;
    case OP_STCR: REG(sReg) = cruMultiBitGet (REG(12), count);          break;

    case OP_SRA:
        i32 = REG (sReg);
        i32 <<= 16;
        i32 >>= count;

        REG (sReg) = i32 >> 16;
        compare (REG (sReg), 0);

        if (i32 & 0x8000)
        {
            carry = 1;
        }

        doCmpZ = 1;
        break;

    case OP_SRC:
        mprintf (LVL_CPU, "SRC : fudging\n");

    case OP_SRL:
        u32 = REG (sReg);
        u32 <<= 16;
        u32 >>= count;

        REG (sReg) = u32 >> 16;
        compare (REG (sReg), 0);

        if (u32 & 0x8000)
        {
            carry = 1;
        }

        doCmpZ = 1;
        break;

    case OP_SLA:
        u32 = REG (sReg);
        u32 <<= count;

        REG (sReg) = u32 & 0xFFFF;
        compare (REG (sReg), 0);

        /*
         *  Set carry flag on MSB set
         */

        if (u32 & 0x10000)
        {
            carry = 1;
        }

        doCmpZ = 1;
        break;

    case OP_BLWP:   blwp (sAddr);                                       break;
    case OP_RTWP:   rtwp ();                                            break;
    case OP_B:      tms9900.pc = sAddr;                                 break;
    case OP_X:      mprintf (LVL_CPU, "X : fudged\n");                        break;
    case OP_CLR:    sData = 0;         doStore=1;                                 break;
    case OP_NEG:    sData = -sData;    doStore=1;   doCmpZ = 1;         break;
    case OP_INV:    sData = ~sData;    doStore=1;   doCmpZ = 1;         break;
    case OP_INC:    sData += 1;        doStore=1;   doCmpZ = 1;         break;
    case OP_INCT:   sData += 2;        doStore=1;   doCmpZ = 1;         break;
    case OP_DEC:    sData -= 1;        doStore=1;   doCmpZ = 1;         break;
    case OP_DECT:   sData -= 2;        doStore=1;   doCmpZ = 1;         break;
    case OP_BL:     REG(11) = tms9900.pc; tms9900.pc = sAddr;           break;
    case OP_SWPB:   sData = SWAP(sData); doStore=1;                     break;
    case OP_SETO:   sData = 0xFFFF;      doStore=1;                     break;
    case OP_ABS:    sData = ((signed) sData < 0) ? -sData : sData; doStore=1;     break;

    default:
        printf ("%04X:%04X\n", pc, data & o->opMask);
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
        if (o->isByte)
        {
            if (dMode)
                memWriteB (dAddr, (WORD) (dData >> 8));
            else
                REG(dReg) = dData & 0xFF00 | REG(dReg) & 0xFF;
        }
        else
        {
            if (dMode)
                memWrite (dAddr, dData);
            else
                REG(dReg) = dData;
        }
    }

    if (doCmpZ)
        compare (dData, 0);

    if (carry)
    {
        tms9900.st |= FLAG_C;
    }

    if (unasmPostExecHook)
    {
        unasmPostExecHook (o->isByte, doStore, dMode, dReg, dAddr, dData, tms9900.wp);
    }
}

void showCPUStatus(void)
{
    WORD i;

    mprintf (LVL_CPU, "CPU\n");
    mprintf (LVL_CPU, "===\n");
    mprintf (LVL_CPU, "st=%04X\nwp=%04X\npc=%04X\n", tms9900.st, tms9900.wp, tms9900.pc);

    for (i = 0; i < 16; i++)
    {
        mprintf (LVL_CPU, "R%02d: %04X ", i, REG(i));
        if ((i + 1) % 4 == 0)
            printf ("\n");
    }
}

void showScratchPad (void)
{
    WORD i, j;

    mprintf (LVL_CPU, "Scratchpad\n");
    mprintf (LVL_CPU, "==========");

    for (i = 0; i < 256; i += 16 )
    {
        mprintf (LVL_CPU, "\n%04X - ", i + 0x8300);

        for (j = i; j < i + 16; j += 2)
        {
            mprintf (LVL_CPU, "%04X ",
                    memRead ((WORD)(j+0x8300)));
        }
    }

    printf ("\n");
}

static void boot (void)
{
    blwp (0x0);  // BLWP @>0
}

static BOOL checkKey (void)
{
    static int c;

    if (kbhit())
    {
        printf ("Key pressed\n");

        c = getch();
    }

    if (!c)
        return 0;

    if (c == 3 || c==32)
    {
        printf ("Break pressed\n");
        c = 0;
        return 1;
    }

    if (c == 'a')
    {
        printf ("Output level bumped down to %d\n", --outputLevel);
        c = 0;
        return 0;
    }

    if (c == 'b')
    {
        printf ("Output level bumped up to %d\n", ++outputLevel);
        c = 0;
        return 0;
    }

    if (tms9900.pc != 0x0478)
        return 0;

    printf ("Forcing key pressed...\n");

    memWriteB(AREG(0), c);
    memWrite(AREG(6), FLAG_EQ);

    showCPUStatus ();

    // keyboardSet (c);
    c = 0;

    return 0;
}

static void input (FILE *fp)
{
    char in[80];
    BOOL run = 0;
    WORD reg, data;
    long execCount = 0;
    long intCount = 0;
    // BOOL cover = 0;

    if (fp)
    {
        fgets (in, sizeof in, fp);
    }
    else
    {
        printf ("\nTMS9900 > ");
        fgets (in, sizeof in, stdin);
        printf ("\n");
    }

    switch (in[0])
    {
    case 'b':
        switch (in[1])
        {
        case 'a':
            breakPointAdd (&in[2]);
            break;
        case 'l':
            breakPointList ();
            break;
        case 'r':
            breakPointRemove (&in[2]);
            break;
        case 'c':
            breakPointCondition (&in[2]);
            break;

        default:
            printf (" * invalid *\n");
            break;
        }
        break;

    case 'w':
        switch (in[1])
        {
        case 'a':
            watchAdd (&in[2]);
            break;
        case 'l':
            watchList ();
            break;
        case 'r':
            watchRemove (&in[2]);
            break;

        default:
            printf (" * invalid *\n");
            break;
        }
        break;

    case 'c':
        switch (in[1])
        {
        case 'a':
            conditionAdd (&in[2]);
            break;
        case 'l':
            conditionList ();
            break;
        case 'r':
            conditionRemove (&in[2]);
            break;
    
        default:
            printf (" * invalid *\n");
            break;
        }
        break;

    case 's':
        showCPUStatus();
        break;
    case 'p':
        showScratchPad ();
        break;
    case 'g':
        run = 1;
        break;

    case 'm':
        sscanf (in+1, " %d %x", &reg, &data);
        memWrite (AREG(reg), (WORD) data);

        run = 1;
        // cover = 1;
        // break;

    case '\n':
        // tms9900.ram.covered[tms9900.pc>>1] = 1;
        execute (fetch());
        showCPUStatus ();
        showGromStatus ();
        break;

    case 'u':
        unasmRunTimeHookAdd();
        if (in[1] == 'v')
        {
            outputCovered = 1;
            mprintf (0, "Uncovered code only will be unassembled\n");
        }
        else
        {
            outputLevel = atoi(&in[1]);
            mprintf (0, "Level set to %d\n", outputLevel);
        }
        break;
    case 'q':
        exit (0);
        break;

    case 'v':
        vdpInitGraphics();
        break;

    case '?':
        printf ("Commands:\n\n");
        break;

    default:
        printf (" * invalid *\n");
        break;
    }

    if (run)
    {
        while (!breakPointHit (tms9900.pc) &&
              // (!cover || tms9900.ram.covered[tms9900.pc>>1] == 1))
              1)
        {
            // tms9900.ram.covered[tms9900.pc>>1] = 1;

            if (checkKey())
                break;

    // gromIntegrity();
            execute (fetch());
    // gromIntegrity();

            if ((execCount++ % 100) == 0)
            {
                // printf ("Refresh....\n");
                vdpRefresh(0);
            }

                /*
                 *  Time for an interrupt ?
                 */

            if ((intCount++ % 100) == 0)
            {
                if ((tms9900.st & FLAG_MSK) > 0)
                {
                    /*
                     *  Clear bit 2 to indicate VDP interrupt
                     */

                    cruBitSet (0, 2, 0);

                    // printf ("Interrupt....\n");
                    blwp (4);
                }
            }

            watchShow();
            // showGromStatus();
        }
    }
}

int main (int argc, char *argv[])
{
    FILE *fp = NULL;

    memLoad ("../994arom.bin",  0x0000, 0x2000);
    memLoad ("../module-c.bin", 0x6000, 0x2000);
    loadGRom ();
    boot ();

    if (argc > 1)
    {
        if ((fp = fopen (argv[1], "r")) == NULL)
        {
            printf ("Can't open '%s'\n", argv[1]);
        }
    }

    while (1)
    {
        input (fp);

        if (feof (fp))
        {
            fclose (fp);
            fp = NULL;
        }
    }

    // return 0;
}

/*
        0x3C00: div
        0x3000: ldcr
        0x2000: coc
        0x2400: czc
        0x3800: mul
        0x0700: abs


|SZC  s,d|4000|-----***|1|Y|Set Zeros Corresponding            |
|SZCB s,d|5000|-----***|1|Y|Set Zeros Corresponding Bytes      |
|S    s,d|6000|---*****|1|N|Subtract                           |
|SB   s,d|7000|--******|1|N|Subtract Bytes                     |
|C    s,d|8000|-----***|1|N|Compare                            |
|CB   s,d|9000|--*--***|1|N|Compare Bytes                      |
|A    s,d|A000|---*****|1|Y|Add                                |
|AB   s,d|B000|--******|1|Y|Add Bytes                          |
|MOV  s,d|C000|-----***|1|Y|Move                               |
|MOVB s,d|D000|--*--***|1|Y|Move Bytes                         |
|SOC  s,d|E000|-----***|1|Y|Set Ones Corresponding             |
|SOCB s,d|F000|-----***|1|Y|Set Ones Corresponding Bytes       |

|LI   r,i|0200|-----***|8|N|Load Immediate                     |
|AI   r,i|0220|---*****|8|Y|Add Immediate                      |
|ANDI r,i|0240|-----***|8|Y|AND Immediate                      |
|ORI  r,i|0260|-----***|8|Y|OR Immediate                       |
|CI   r,i|0280|-----***|8|N|Compare Immediate                  |
|STST r  |02C0|--------|8|N|Store Status Register              |
|STWP r  |02A0|--------|8|N|Store Workspace Pointer            |
|LWPI i  |02E0|--------|8|N|Load Workspace Pointer Immediate   |
|LIMI i  |0300|*-------|8|N|Load Interrupt Mask Immediate      |

|IDLE    |0340|--------|7|N|Computer Idle                      |
|RSET    |0360|*-------|7|N|Reset                              |
|RTWP    |0380|????????|7|N|Return Workspace Pointer (4)       |
|CKON    |03A0|--------|7|N|Clock On                           |
|CKOF    |03C0|--------|7|N|Clock Off                          |
|LREX    |03E0|*-------|7|N|Load or Restart Execution          |

|BLWP s  |0400|--------|6|N|Branch & Load Workspace Ptr (3) (2)|
|B    s  |0440|--------|6|N|Branch (PC=d)                      |
|X    s  |0480|--------|6|N|Execute the instruction at s       |
|CLR  d  |04C0|--------|6|N|Clear                              |
|NEG  d  |0500|---*****|6|Y|Negate                             |
|INV  d  |0540|-----***|6|Y|Invert                             |
|INC  d  |0580|---*****|6|Y|Increment                          |
|INCT d  |05C0|---*****|6|Y|Increment by Two                   |
|DEC  d  |0600|---*****|6|Y|Decrement                          |
|DECT d  |0640|---*****|6|Y|Decrement by Two                   |
|BL   s  |0680|--------|6|N|Branch and Link (R11=PC,PC=s)      |
|SWPB d  |06C0|--------|6|N|Swap Bytes                         |
|SETO d  |0700|--------|6|N|Set to Ones                        |
|ABS  d  |0740|---*****|6|Y|Absolute value                     |

|SRA  r,c|0800|----****|5|Y|Shift Right Arithmetic (1)         |
|SRC  r,c|0800|----****|5|Y|Shift Right Circular (1)           |
|SRL  r,c|0900|----****|5|Y|Shift Right Logical (1)            |
|SLA  r,c|0A00|----****|5|Y|Shift Left Arithmetic (1)          |

|JMP  a  |1000|--------|2|N|Jump unconditionally               |
|JLT  a  |1100|--------|2|N|Jump if Less Than                  |
|JLE  a  |1200|--------|2|N|Jump if Low or Equal               |
|JEQ  a  |1300|--------|2|N|Jump if Equal                      |
|JHE  a  |1400|--------|2|N|Jump if High or Equal              |
|JGT  a  |1500|--------|2|N|Jump if Greater Than               |
|JNE  a  |1600|--------|2|N|Jump if Not Equal                  |
|JNC  a  |1700|--------|2|N|Jump if No Carry                   |
|JOC  a  |1800|--------|2|N|Jump On Carry                      |
|JNO  a  |1900|--------|2|N|Jump if No Overflow                |
|JL   a  |1A00|--------|2|N|Jump if Low                        |
|JH   a  |1B00|--------|2|N|Jump if High                       |
|JOP  a  |1C00|--------|2|N|Jump if Odd Parity                 |
|SBO  a  |1D00|--------|2|N|Set Bit to One                     |
|SBZ  a  |1E00|--------|2|N|Set Bit to Zero                    |
|TB   a  |1F00|-----*--|2|N|Test Bit                           |

|COC  s,r|2000|-----*--|3|N|Compare Ones Corresponding         |
|CZC  s,r|2400|-----*--|3|N|Compare Zeros Corresponding        |
|XOR  s,r|2800|-----***|3|N|Exclusive OR                       |

|XOP  s,c|2C00|-1------|9|N|Extended Operation (5) (2)         |
|LDCR s,c|3000|--*--***|4|Y|Load Communication Register        |
|STCR s,c|3400|--*--***|4|Y|Store Communication Register       |
|MPY  d,r|3800|--------|9|N|Multiply                           |
|DIV  d,r|3C00|---*----|9|N|Divide                             |
*/
