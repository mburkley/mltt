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
 *  GPL disassembler.  Maintains a state machine that interprets GPL bytes as
 *  they are fed in.  This is incomplete and frequently gets misaligned on where
 *  instructions begin.
 *
 *  Interpreted using http://aa-ti994a.oratronik.de/gpl_programmers_guide.pdf
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "trace.h"
#include "gpl.h"

typedef struct
{
    int value;
    const char *mme;
} MME;

static MME mnemonics[] =
{
    { 0x00, "RTN" },
    { 0x01, "RTNC" },
    { 0x02, "RAND" },
    { 0x03, "SCAN" },
    { 0x04, "BACK" },
    { 0x05, "B" },
    { 0x06, "CALL" },
    { 0x07, "ALL" },
    { 0x08, "FMT" },
    { 0x09, "H" },
    { 0x0A, "GT" },
    { 0x0B, "EXIT" },
    { 0x0C, "CARRY" },
    { 0x0D, "OVF" },
    { 0x0E, "PARSE" },
    { 0x0F, "XML" },
    { 0x10, "CONT" },
    { 0x11, "EXEC" },
    { 0x12, "RTNB" },
    { 0x20, "MOVE" },
    { 0x40, "BR" },
    { 0x60, "BS" },
    { 0x80, "ABS" },
    { 0x82, "NEG" },
    { 0x84, "INV" },
    { 0x86, "CLR" },
    { 0x88, "FETCH" },
    { 0x8A, "CASE" },
    { 0x8C, "PUSH" },
    { 0x8E, "CZ" },
    { 0x90, "INC" },
    { 0x92, "DEC" },
    { 0x94, "INCT" },
    { 0x96, "DECT" },
    { 0xA0, "ADD" },
    { 0xA4, "SUB" },
    { 0xA8, "MUL" },
    { 0xAC, "DIV" },
    { 0xB0, "AND" },
    { 0xB4, "OR" },
    { 0xB8, "XOR " },
    { 0xBC, "ST" },
    { 0xC0, "EX" },
    { 0xC4, "CH" },
    { 0xC8, "CHE" },
    { 0xCC, "CGT" },
    { 0xD0, "CGE" },
    { 0xD4, "CEQ" },
    { 0xD8, "CLOG" },
    { 0xDC, "SRA" },
    { 0xE0, "SLL" },
    { 0xE4, "SRL" },
    { 0xE8, "SRC" },
    { 0xEC, "COIN" },
    { 0xF4, "I/O" },
    { 0xF8, "SWGR" }
};

#define OPERAND_STATE_SHORT     1
#define OPERAND_STATE_LONG      2
#define OPERAND_STATE_EXTENDED  3

struct _scratchPadUsage
{
    const char *mnemonic;
    uint16_t addrStart;
    uint16_t addrEnd;
    const char *description;
}
scratchPadUsage[] =
{
    { "XXXXXX", 0x8300, 0x834A, "Unknown general scratch pad area" },
    { "FAC", 0x834A, 0x8352, "Floating point accumulator (8 bytes)" },
    { "PAB", 0x8352, 0x8354, "PAB copy area" },
    { "FLTERR", 0x8354,0x8356, "Floating point error code / Size of DSR name" },
    { "DSRNAM", 0x8356,0x835C, "Pointer to DSR name for LINK" },
    { "ARG", 0x835C, 0x8364, "Floating point argument (8 bytes)" },
    { "XXXXXX", 0x8364, 0x836E, "Unknown general scratch pad area" },
    { "VAL", 0x836E,0x8370, "Pointer to real numbers value stack in VDP mem" },
    { "FREE", 0x8370,0x8372, "Highest free VDP memory address" },
    { "VLPTR", 0x8372,0x8373, "Pointer to top of value stack (in pad)" },
    { "SBPTR", 0x8373,0x8374, "Pointer to top of subroutine stack (in pad)" },
    { "MODE", 0x8374,0x8375, "Keyboard scanning mode" },
    { "KEY", 0x8375,0x8376, "Code if the key detected (>FF = none) "
                            "Also: sign of a real number" },
    { "JOYY", 0x8376,0x8377, "Joystick vertical value (4, 0, >FC) "
                             "Also: exponent of a real number" },
    { "JOYX", 0x8377,0x8378, "Joystick horizontal value (4, 0, >FC)" },
    { "RANDOM", 0x8378,0x8379, "Random number, found after RND" },
    { "TIMER", 0x8379,0x837A, "VDP interrupt timer" },
    { "AUTO", 0x837A,0x837B, "Highest sprite in auto-motion" },
    { "VDPSTS", 0x837B,0x837C, "Copy of VDP status byte" },
    { "GPLSTS", 0x837C,0x837D, "GPL status byte" },
    { "CCHA", 0x837D,0x837E, "Char at current screen position" },
    { "CROW", 0x837E,0x837F, "Current screen row" },
    { "CCOL", 0x837F,0x8380, "Current screen column" },
    { "SBSTA", 0x8380, 0x83A0, "Area reserved for subroutine stack "
                               "(32 bytes, first 9 for Basic)" },
    { "VLSTA", 0x83A0, 0x83C0, "Area reserved for data stack (32 bytes)" },
    /*  To avoid an address belonging to multiple ranges, I have given the more
     *  specific usage where possible and only used INTWS and other generic
     *  labels where a specific register usage isn't documented
     */
    { "RNDSD", 0x83C0,0x83C2, "Random number seed" },
    { "AMSQ", 0x83C2,0x83C4, "ISR disabling flags: >80 All, "
                             ">40 Motion, >20 Sound, >10 Quit key" },
    { "ISR", 0x83C4,0x83C6, "Interrupt Service Routine hook: "
                            "routine to be executed" },
    { "INTWS", 0x83C6,0x83CC, "Interrupt routine workspace "
                              "(32 bytes >83C0 to >83E0)" },
    { "SNDTAB", 0x83CC,0x83CE, "Address of the sound table" },
    { "SNDSIZ", 0x83CE,0x83D0, "Sound bytes to play (>0100)" },
    { "INTWS", 0x83D0,0x83D4, "Interrupt routine workspace "
                              "(32 bytes >83C0 to > 83E0)" },
    { "VDPR1", 0x83D4,0x83D6, "Copy of VDP register 1, used by SCAN" },
    { "CLRSC", 0x83D6,0x83D8, "Screen timeout counter: decremented by 2, "
                              "clears when 0" },
    { "SCNRTN", 0x83D8,0x83DA, "Return address saved by SCAN." },
    { "INTWS", 0x83DA,0x83E0, "to >83DF used for RTWP (workspace, "
                              "pc and status)." },
    { "GPLWS", 0x83E0,0x83FA, "GPL interpreter workspace" },
    { "GBASE", 0x83FA,0x83FC, "GROM port currently used (normally >9800)" },
    { "SPEED", 0x83FC,0x83FD, "Speed value, added to TIMER" },
    { "FLAGS", 0x83FD,0x83FE, ">20 cassette operations, >10 cassette verify,"
                              " >08 16K VDP mem >02 multicolor mode, >01 sound table in VDP mem" },
    { "VDPWAP", 0x83FE,0x8400, "VDP write address port (>8C02)." }
};

#define NSCRATCH (sizeof (scratchPadUsage) / sizeof (struct _scratchPadUsage))

typedef struct
{
    int state;
    int bytesNeeded;
    int bytesStored;
    bool indexNeeded;
    bool indexStored;
    int index;
    int immed;
    bool cpu;
    bool vdp;
    bool extended;
    bool indirect;
    bool romIndexed;
    int bytesMovedImmed;
    int addr;
}
gplOperand;

static struct
{
    int addr;
    int bytesNeeded;
    int bytesStored;
    int opCode;
    int length;
    int lengthBytesNeeded;
    int lengthBytesStored;
    int immed;
    bool immedNeeded;
    bool immedStored;
    bool multiByte;
    bool fmtMode;
    gplOperand src;
    gplOperand dst;
    uint8_t operation[10];
}
gplState;

/*  Show a src or dst address.  Could be CPU, VDP or immed
 */
static void showAddress (gplOperand *op)
{
    #if 0
    if (op->cpu && op->vdp)
        printf ("*** can't be CPU and VDP?");
    #endif

    // printf("move, dst cpu is %d\n", gplState.dst.cpu);

    if (op->indirect && op->vdp)
        mprintf (LVL_GPL, "*VDP(%04X)", op->addr + 0x8300);
    else if (op->vdp)
        mprintf (LVL_GPL, "VPD(>%04X)", op->addr);
    else if (op->cpu) //  && !op->extended)  TODO is CPU always offset by 8300 ???
        mprintf (LVL_GPL, ">%04X", op->addr + 0x8300);
    else if (gplState.multiByte)
        mprintf (LVL_GPL, ">%04X", op->addr);
    else
        mprintf (LVL_GPL, ">%02X", op->addr);

}

static void interpret (uint16_t cpuPC)
{
    int i;
    const char *m = "????";

    for (i = 0; i < (int) (sizeof (mnemonics) / sizeof (MME)); i++)
        if (mnemonics[i].value == gplState.opCode)
            m = mnemonics[i].mme;

    mprintf (LVL_GPL, "PC:%04X GR:%04X :",
             cpuPC,
             gplState.addr);

    for (i = 0; i < gplState.bytesStored; i++)
        mprintf (LVL_GPL, " %02X", gplState.operation[i]);

    for (; i < 6; i++)
        mprintf (LVL_GPL, "   ");

    if (gplState.multiByte)
        mprintf (LVL_GPL, " D%-5.5s ", m);
    else
        mprintf (LVL_GPL, " %-6.6s ", m);

    if (gplState.immedNeeded)
        mprintf (LVL_GPL, ">%02X", gplState.immed);

    else if (gplState.dst.bytesNeeded > 0)
    {
        /*  MOVE instruction */
        if (gplState.lengthBytesStored > 0)
            mprintf (LVL_GPL, "%d from ", gplState.length);

        showAddress (&gplState.src);
        mprintf (LVL_GPL, ",");
        showAddress (&gplState.dst);
    }
    else if (gplState.src.bytesNeeded > 0)
        showAddress (&gplState.src);

    mprintf (LVL_GPL, "\n");
    mprintf (LVL_GPLDBG, "\n");
    /*  Clear the decode state machine */
    gplState.bytesStored = 0;
    gplState.bytesNeeded = 0;
}

/*  Returns 1 byte was consumed or 0 if don't want it */
static int decodeOperand (gplOperand *op, uint8_t data)
{
    mprintf(LVL_GPLDBG, "Process operand %s\n", op==&gplState.src ? "SRC" : "DST");
    /*  We don't do bitwise interpretation of values if they are immediate, they
     *  are always fixed length.  Otherwise we figure out how long the address
     *  is based on the bit pattern of the first byte.
     *
     */
    if (op->immed == false && op->bytesNeeded == 1 && op->bytesStored == 0)
    {
        mprintf(LVL_GPLDBG, "first byte of operand %02X\n", data);
        if (data & 0x80)
        {
            gplState.bytesNeeded++;
            mprintf(LVL_GPLDBG, "long addr, need %d bytes\n", gplState.bytesNeeded);
            op->bytesNeeded = 2;

            if (data & 0x20)
            {
                op->vdp = true;
                // op->cpu = false;
            }
            else        
                op->cpu = true;

            if (data & 0x10)
                op->indirect = true;

            if (data & 0x40)
            {
                mprintf(LVL_GPLDBG, "needs index\n");
                op->indexNeeded = true;
            }

            if ((data & 0x0f) == 0x0f)
            {
                /*  Extended addressing, from >0000 to >FFFF */
                gplState.bytesNeeded++;
                mprintf (LVL_GPLDBG, "extended, need %d bytes\n", gplState.bytesNeeded);
                op->extended = true;
                op->bytesNeeded = 3;
                op->addr = 0;
            }
            else
            {
                /* Addressing CPU >8380 to >83FF or VDP >0000 to >0FFF */
                op->addr = data & 0x0F;
            }

            op->bytesStored = 1;
            return 1;
        }
        else
        {
            /*  Short addressing >8300 to >837F */
            mprintf(LVL_GPLDBG, "short addr\n");
            op->addr = data & 0x7F;
            op->bytesStored = 1;
            if (!op->vdp)
                op->cpu = true;
            return 1;
        }
    }
    else if (op->bytesNeeded > op->bytesStored)
    {
        mprintf(LVL_GPLDBG, "storing byte %02X, remain=%d\n", data, op->bytesNeeded - op->bytesStored);
        op->addr = (op->addr << 8) | data;
        op->bytesStored++;
        return 1;
    }
    else if (op->indexNeeded && !op->indexStored)
    {
        op->index = data;
        op->indexStored = true;
        return 1;
    }

    return 0;
}

static void decodeNextByte (uint8_t data)
{
    /*  Decoding second and subsequent bytes of an instruction */
    gplState.operation[gplState.bytesStored++] = data;

    mprintf(LVL_GPLDBG, "length is %d/%d\n", gplState.lengthBytesStored, gplState.lengthBytesNeeded);
    if (gplState.lengthBytesStored < gplState.lengthBytesNeeded)
    {
        mprintf(LVL_GPLDBG, "store length byte %d\n", gplState.lengthBytesStored);
        gplState.length = (gplState.length << 8) | data;
        gplState.lengthBytesStored++;
        return;
    }
    else if (decodeOperand (&gplState.dst, data) == 1)
        return;
    else if (decodeOperand (&gplState.src, data) == 1)
        return;
    else if (gplState.immedNeeded && !gplState.immedStored)
    {
        gplState.immed = (gplState.immed << 8) | data;
        gplState.immedStored = true;
        return;
    }
}

static void decodeFirstByte (uint16_t addr, uint8_t data)
{
    /*  Decoding first byte of an instruction, reset the state and store the
     *  first byte
     */
    memset (&gplState, 0, sizeof (gplState));
    gplState.addr = addr;
    gplState.operation[0] = data;
    gplState.bytesStored = 1;

    if (data >= 0xa0)
    {
        /*  Opcodes >= 0xA0 have src and a dst operands.
         */
        gplState.opCode = data & 0xfc;
        gplState.dst.bytesNeeded = 1;  // Assume one byte for now
        gplState.src.bytesNeeded = 1;  // Assume one byte for now
        gplState.bytesNeeded = 3;    // Need at least 3 bytes to decode
        gplState.multiByte = data & 0x01;
        gplState.src.immed = data & 0x02;

        // if (!gplState.src.immed)
        //     gplState.src.cpu = true; // If not immed then assume src is CPU

        // gplState.dst.cpu = true; // dest is always CPU

        /*  If this is a multi byte operation with an immediate src then the src
         *  size must be 2 bytes
         */
        if (gplState.multiByte && gplState.src.immed)
        {
            gplState.src.bytesNeeded = 2;  // Src must have 2 bytes
            gplState.bytesNeeded = 4;    // Need at least 4 bytes to decode
        }
    }
    else if (data >= 0x80)
    {
        gplState.opCode = data & 0xfe;
        gplState.src.bytesNeeded = 1;  // Assume one byte for now
        gplState.bytesNeeded = 2;    // Need at least 3 bytes to decode
        gplState.multiByte = data & 0x01;
    }
    else if (data >= 0x40)
    {
        gplState.opCode = data & 0xe0;
        gplState.immed = data & 0x1f;

        /* TODO 0x40 0xDC is a valid isntrcution???  BR 0xDC ?? */
        /* Single byte instruction with an immediate */
        gplState.bytesNeeded = 2;
        gplState.immedNeeded = true;
    }
    else if (data >= 0x20)
    {
        mprintf(LVL_GPLDBG, "move, len=2\n");
        gplState.opCode = data & 0xe0;
        /* Followed by length, GD and GS */
        gplState.lengthBytesNeeded = 2;
        gplState.dst.bytesNeeded = 1;  // Assume one byte for now
        gplState.src.bytesNeeded = 1;  // Seems move src is asumed 2 bytes?
        gplState.bytesNeeded = 5;    // Need at least 5 bytes to decode
        gplState.dst.cpu = !(data & 0x10);
        // printf("move, dst cpu is %d\n", gplState.dst.cpu);
        gplState.dst.vdp = data & 0x08;
        gplState.src.cpu = data & 0x04; // src is CPU RAM
        if (gplState.src.cpu == 0)
        {
            mprintf(LVL_GPLDBG, "src!=cpu ram, assume long src\n");
            gplState.src.bytesNeeded++;
            gplState.bytesNeeded++;
        }
        gplState.src.romIndexed = data & 0x02;
        gplState.src.bytesMovedImmed = data & 0x01;
        if (gplState.dst.bytesMovedImmed) mprintf(LVL_GPLDBG, "move immed\n");
    }
    else
    {
        gplState.opCode = data;
        switch (gplState.opCode)
        {
        case 0x00:
        case 0x03:
            /* RTN,SCAN take no params */
            gplState.bytesNeeded = 1;
            break;
        case 0x08:
            /* FMT take no params, ignore more data until 0xFB is seen */
            gplState.bytesNeeded = 1;
            gplState.fmtMode = true;
            break;
        case 0x05:
        case 0x06:
            /*  CALL and B take a 2-byte label, others take 1-byte immed */
            /* Followed by 2-byte label */
            gplState.src.bytesNeeded = 2;
            gplState.bytesNeeded = 3;
            break;
        default:
            /* Single byte instruction with an immediate */
            gplState.bytesNeeded = 2;
            gplState.immedNeeded = true;
            break;
        }
    }
}

extern TMS9900 cpu;

void gplDisassemble (uint16_t addr, uint8_t data)
{
    /*  Ignore a byte fetch if CPU address is 0x7A as this is just checking to
     *  see if a GROM is present 
     */
    mprintf (LVL_GPLDBG, "process byte from cpu=%04X gr=%04X data=%02X\n",
            cpu.getPC(), addr, data);

    if (cpu.getPC() == 0x5E) // Looking for 0xAA signature, not an instruction
        return;

    if (cpu.getPC() == 0x680) // Fetcing bytes fro MOVE, not an instruction
        return;

    if (cpu.getPC() == 0x041E) // TODO unknown fetch
        return;

    if (cpu.getPC() == 0x0A26) // Sound data
        return;

    if (cpu.getPC() == 0x0A34) // Sound data
        return;

    if (cpu.getPC() == 0x0A3E) // Sound data ?
        return;

    if (gplState.fmtMode && data == 0xFB)
    {
        mprintf (LVL_GPL, "    FMT >FB end\n");
        gplState.fmtMode = false;
        return;
    }

    if (gplState.fmtMode)
    {
        mprintf (LVL_GPL, "    FMT >%02X\n", data);
        return;
    }

    mprintf (LVL_GPLDBG, "have %d/%d, decode next\n", gplState.bytesStored, gplState.bytesNeeded);

    if (gplState.bytesStored == 0)
        decodeFirstByte (addr, data);
    else if (gplState.bytesNeeded > gplState.bytesStored)
        decodeNextByte (data);

    mprintf (LVL_GPLDBG, "after decode have %d/%d\n", gplState.bytesStored, gplState.bytesNeeded);

    /*  If we have a full instruction, interpret it */
    if (gplState.bytesNeeded == gplState.bytesStored)
        interpret(cpu.getPC());
}

void gplShowScratchPad (uint16_t addr)
{
    for (unsigned i = 0; i < NSCRATCH; i++)
    {
        struct _scratchPadUsage *pad = &scratchPadUsage[i];

        if (pad->addrStart <= addr &&
            pad->addrEnd > addr)
        {
            printf ("%-6.6s %s", pad->mnemonic, pad->description);
            return;
        }
    }
}

uint16_t gplScratchPadNext (uint16_t addr)
{
    for (unsigned i = 0; i < NSCRATCH; i++)
    {
        struct _scratchPadUsage *pad = &scratchPadUsage[i];

        if (pad->addrStart <= addr &&
            pad->addrEnd > addr)
        {
            return pad->addrEnd;
        }
    }
    return 0;
}

