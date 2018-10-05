#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <conio.h>

#include "vdp.h"
#include "cpu.h"
#include "break.h"
#include "watch.h"
#include "cond.h"

struct
{
    WORD pc;
    WORD wp;
    WORD st;

    union
    {
        // WORD w[32768];
        // BYTE b[65536];
        WORD w[0x5000];
        BYTE b[0xA000];
    }
    ram;
}
tms9900;

struct
{
    WORD addr;
    BYTE lowByteGet;
    BYTE lowByteSet;

    union
    {
        // WORD w[32768];
        // BYTE b[65536];
        WORD w[0x2000];
        BYTE b[0x4000];
    }
    rom;
}
gRom;

BOOL quiet;

#define AREG(n)  (WORD)(tms9900.wp+(n<<1))
#define REG(n)  cpuRead(AREG(n))
#define SWAP(w)  ((w >> 8) | ((w & 0xFF) << 8))

static int mprintf (char *s, ...)
{
    va_list ap;

    va_start (ap, s);
    if (!quiet)
        vprintf (s, ap);
    va_end (ap);

    return 0;
}

void halt (char *s)
{
    vdpRefresh(1);

    fprintf (stderr, "HALT: %s\n", s);

    exit (1);
}

static BYTE gromRead (void)
{
    printf ("GROMRead: %04X : %02X\n",
            (unsigned) gRom.addr,
            (unsigned) gRom.rom.b[gRom.addr]);

    gRom.lowByteGet = 0;
    gRom.lowByteSet = 0;
    mprintf ("GROMAD lo byte Clr\n");

    return gRom.rom.b[gRom.addr++];
}

static void gromSetAddr (WORD addr)
{
    if (gRom.lowByteSet)
    {
        gRom.addr = gRom.addr & 0xFF00 | addr;
        gRom.lowByteSet = 0;
        mprintf ("GROMAD addr set to %04X\n", gRom.addr);
    }
    else
    {
        gRom.addr = addr << 8;
        gRom.lowByteSet = 1;
        mprintf ("GROMAD lo byte Set\n");
    }
}

static BYTE gromGetAddr (void)
{
    if (gRom.lowByteGet)
    {
        gRom.lowByteGet = 0;
        mprintf ("GROMAD addr get as %04X\n", gRom.addr + 1);
        return (gRom.addr + 1) & 0xFF;
    }

    gRom.lowByteGet = 1;
    mprintf ("GROMAD lo byte Get\n");
    return (gRom.addr + 1) >> 8;
}

WORD cpuRead(WORD addr)
{
    if ((addr & 0xF800) == 0x9800)
    {
        halt ("Invalid word read from GROM");
    }
    else if ((addr & 0xF800) == 0x8800)
    {
        halt ("Invalid word read from VDP");
    }

    return tms9900.ram.w[addr>>1];
}

static void cpuWrite(WORD addr, WORD data)
{
    /*
    if (addr < 0x2000)
    {
        halt ("Write to ROM");
    }
    */

    if ((addr & 0xF800) == 0x9800)
    {
        if (addr == 0x9802)
        {
            mprintf ("GROMAD WORD write to 9802\n");
            gromSetAddr (data);
        }
        else
        {
            halt ("Invalid word write to GROM");
        }
    }
    else if ((addr & 0xF800) == 0x8800)
    {
        halt ("Invalid word write to GROM");
    }

    tms9900.ram.w[addr>>1] = data;
}

static BYTE cpuReadB(WORD addr)
{
    if ((addr & 0xF800) == 0x9800)
    {
        if (addr == 0x9800)
        {
            tms9900.ram.b[0x9801] = gromRead ();
        }
        else if (addr == 0x9802)
        {
            tms9900.ram.b[0x9803] = gromGetAddr ();
        }
    }
    else if ((addr & 0xF800) == 0x8800)
    {
        if (addr == 0x8800)
        {
            tms9900.ram.b[0x8801] = vdpReadData ();
        }
        else if (addr == 0x8802)
        {
            tms9900.ram.b[0x8803] = vdpReadStatus ();
        }
    }

    addr = addr ^ 1;

    return tms9900.ram.b[addr];
}

static void cpuWriteB(WORD addr, BYTE data)
{
    /*
    if (addr < 0x2000)
    {
        halt ("Write to ROM");
    }
    */

    if ((addr & 0xF800) == 0x9800)
    {
        if (addr == 0x9802)
        {
            halt ("Invalid byte write to GROM addr");
            gromSetAddr (data);
        }
        else if (addr == 0x9C02)
        {
            mprintf ("GROMAD BYTE write to 9802\n");
            gromSetAddr (data);
        }
        else
        {
            halt ("invalid GROM write operation");
        }
    }
    else if ((addr & 0xF800) == 0x8800)
    {
        if (addr == 0x8C00)
        {
            vdpWriteData (data);
        }
        else if (addr == 0x8C02)
        {
            vdpWriteCommand (data);
        }
        else
        {
            halt ("invalid VDP write operation");
        }
    }

    addr = addr ^ 1;

    tms9900.ram.b[addr] = data;
}

static WORD fetch (void)
{
    WORD ret;

    if (tms9900.pc >= 0x2000)
        halt ("Fetch from outside ROM area\n");

    ret = cpuRead(tms9900.pc);
    tms9900.pc += 2;
    return ret;
}

static void blwp (WORD addr)
{
    WORD owp = tms9900.wp;
    WORD opc = tms9900.pc;

    mprintf ("blwp@%x\n", addr);

    tms9900.wp = cpuRead (addr);
    tms9900.pc = cpuRead ((WORD) (addr+2));

    cpuWrite (AREG (13), owp);
    cpuWrite (AREG (14), opc);
    cpuWrite (AREG (15), tms9900.st);
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

    mprintf ("CMP: %04X : %04X %s %s %s %s\n",
            data1, data2,
            tms9900.st & FLAG_C ? "C" : "",
            tms9900.st & FLAG_EQ ? "EQ" : "",
            tms9900.st & FLAG_LGT ? "LGT" : "",
            tms9900.st & FLAG_AGT ? "AGT" : "");

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

    mprintf ("CMPB: %02X : %02X %s %s %s\n",
            data1, data2,
            tms9900.st & FLAG_EQ ? "EQ" : "",
            tms9900.st & FLAG_LGT ? "LGT" : "",
            tms9900.st & FLAG_AGT ? "AGT" : "");

}

static WORD decode (WORD data,
                    BOOL *isByte,
                    WORD *sReg,
                    WORD *sMode,
                    WORD *dReg,
                    WORD *dMode,
                    I8 *offset,
                    WORD *count)
{
    WORD opcode;

    if (data & 0xC000)
    {
        /*
         *  FMT 1
         */

        *isByte =  data & 0x1000;
        *dMode  = (data & 0x0C00) >> 10;
        *dReg   = (data & 0x03C0) >> 6;
        *sMode  = (data & 0x0030) >> 4;
        *sReg   =  data & 0x000F;
        opcode =  data & 0xE000;

        mprintf ("2-op : %04X\n", opcode);
    }
    else if (data & 0x2000)
    {
        // printf ("coc/czc\n");

        *dReg   = (data & 0x03C0) >> 6;
        *sMode  = (data & 0x0030) >> 4;
        *sReg   =  data & 0x000F;
        opcode =  data & 0xFC00;
    }
    else if (data & 0x1000)
    {
        // printf ("jmp\n");

        *offset = data & 0x00FF;
        opcode = data & 0xFF00;
    }
    else if (data & 0x0800)
    {
        // printf ("shift\n");

        *count  = (data & 0x00F0) >> 4;
        *sReg   =  data & 0x000F;
        opcode =  data & 0xFF00;
    }
    else if (data & 0x0400)
    {
        // printf ("prog\n");

        *sMode  = (data & 0x0030) >> 4;
        *sReg   =  data & 0x000F;
        opcode =  data & 0xFFC0;
    }
    else
    {
        // printf ("immed, pc is %x\n", tms9900.pc << 1);

        *sReg   = data & 0x000F;
        opcode = data & 0xFFF0;
    }

    // printf ("dm=%d, dreg=%d, sm=%d, sreg=%d, byte=%d, pc=%x\n",
    //         dMode, dReg, sMode, sReg, isByte, tms9900.pc);

    return opcode;
}

static void operand (WORD mode, WORD reg, WORD *addr, BOOL isByte)
{
    switch (mode)
    {
    case AMODE_NORMAL:
        *addr = AREG(reg);
        break;

    case AMODE_INDIR:
        mprintf ("INDIR: R%d->%04X\n", reg, REG(reg));
        *addr = REG(reg);
        break;

    case AMODE_SYM:
        // printf ("SYM: @[%04X]+R%d (%04X) \n",
        //         (unsigned) cpuRead (tms9900.pc), (unsigned) sReg, (unsigned) REG (sReg));
        *addr = (WORD) (fetch() + (reg == 0 ? 0 : REG (reg)));
        break;

    case AMODE_INDIRINC:
        *addr = REG(reg);
        cpuWrite (AREG(reg), (WORD) (REG(reg) + (isByte ? 1 : 2)));
        break;
    }

    // printf ("dm=%d, sm=%d, pc=%x\n", dMode, sMode, tms9900.pc);

}

static void execute (WORD data)
{
    WORD  sReg = 0, dReg = 0;
    WORD sAddr = 0, dAddr = 0;
    WORD sData = 0, dData = 0;
    WORD dMode = 0, sMode = 0;
    BOOL isByte = 0;
    BOOL doStore = 0;
    I8 offset = 0;
    WORD count = 0;
    WORD opcode = 0;
    U32 u32 = 0;
    I32 i32 = 0;
    BOOL carry = 0;

    mprintf ("\n%04X:%04X\n\n", tms9900.pc - 2, data);

    opcode = decode (data,
            &isByte, &sReg,
            &sMode,
            &dReg,
            &dMode,
            &offset,
            &count);

    operand (sMode, sReg, &sAddr, isByte);
    operand (dMode, dReg, &dAddr, isByte);

    if (isByte)
    {
        sData = cpuReadB (sAddr) << 8;
        dData = cpuReadB (dAddr) << 8;

        mprintf ("sData [%04X] = %02X, dData [%04X] = %02X\n",
                (unsigned) sAddr, (unsigned) sData >> 8,
                (unsigned) dAddr, (unsigned) dData >> 8);
    }
    else
    {
        sData = cpuRead (sAddr);
        dData = cpuRead (dAddr);

        mprintf ("sData = %04X, dData = %04X\n", (unsigned) sData, (unsigned) dData);
    }

    switch (opcode)
    {

    /*
     *  D U A L   O P E R A N D
     */

    case OP_SZC:
        dData = dData & ~sData;
        doStore = 1;
        break;

    case OP_S:
        dData = dData - sData;
        doStore = 1;
        break;

    case OP_C:
        // if (isByte)
        // {
        //     compareB (sData, dData);
        // }
        // else
        // {
            compare (sData, dData);
        // }

        break;

    case OP_A:
        mprintf ("Add %04X+%04X\n", sData, dData);
        u32 = (U32) dData + sData;
        dData = u32 & 0xFFFF;
        u32 >>= 16;

        if (u32)
            carry = 1;

        doStore = 1;
        break;

    case OP_MOV:
        mprintf ("MOV: %04X\n", (unsigned) sData);
        dData = sData;
        doStore = 1;
        break;

    case OP_SOC:
        dData = dData | sData;
        doStore = 1;
        break;

    /*
     *
     */

    case OP_COC:
        compare (sData & dData, sData);
        break;

    case OP_CZC:
        compare (~sData & dData, sData);
        break;

    case OP_XOR:
        dData ^= sData;
        doStore = 1;
        break;

    case OP_XOP:
        halt ("Unsupported");
    case OP_MPY:
        /*
         *  TODO: double-check this
         */
        u32 = dData * sData;
        cpuWrite (AREG(dReg), (WORD) (u32 >> 16));
        cpuWrite (AREG(dReg+1), (WORD) (u32 & 0xFFFF));
        break;

    case OP_DIV:
        /*
         *  TODO: double-check this
         */
        u32 = REG(dReg) << 16 | REG(dReg+1);
        u32 /= sData;
        cpuWrite (AREG(dReg), (WORD) (u32 >> 16));
        cpuWrite (AREG(dReg+1), (WORD) (u32 & 0xFFFF));
        break;

    /*
     *  I M M E D I A T E S
     */

    case OP_LI:
        mprintf ("LI:%x, pc is %x\n",
                cpuRead(tms9900.pc+2),
                tms9900.pc);
        cpuWrite (AREG(sReg), fetch());
        break;

    case OP_AI:
		cpuWrite (AREG(sReg), (WORD) (REG(sReg) + fetch()));
		break;

	case OP_ANDI:
		cpuWrite (AREG(sReg), (WORD) (REG(sReg) & fetch()));
        break;

    case OP_ORI:
		cpuWrite (AREG(sReg), (WORD) (REG(sReg) | fetch()));
        break;

    case OP_CI:
        compare (sData, fetch());
        break;

    case OP_STST:
        cpuWrite (AREG(sReg), tms9900.st);
        break;

    case OP_STWP:
        cpuWrite (AREG(sReg), tms9900.wp);
        break;

    case OP_LWPI:
        tms9900.wp = fetch();
        break;

    case OP_LIMI:
        tms9900.st = (tms9900.st & 0x0FFF) | fetch();
        break;

    /*
     *  J U M P
     */

    case OP_JMP:
        tms9900.pc += offset << 1;
        break;

    case OP_JLT:
        if (!(tms9900.st & FLAG_AGT) &&
            !(tms9900.st & FLAG_EQ))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JGT:
        if ((tms9900.st & FLAG_AGT))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JL:
        if (!(tms9900.st & FLAG_LGT) &&
            !(tms9900.st & FLAG_EQ))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JLE:
        if (!(tms9900.st & FLAG_LGT) &&
             (tms9900.st & FLAG_EQ))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JH:
        if ( (tms9900.st & FLAG_LGT) &&
            !(tms9900.st & FLAG_EQ))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JHE:
        if ( (tms9900.st & FLAG_LGT) &&
             (tms9900.st & FLAG_EQ))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JNC:
        mprintf ("JNC : %s\n", tms9900.st & FLAG_C ? "Won't" : "Will");
        if (!(tms9900.st & FLAG_C))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JOC:
        mprintf ("JOC : %s\n", tms9900.st & FLAG_C ? "Will" : "Won't");
        if ((tms9900.st & FLAG_C))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JNE:
        if (!(tms9900.st & FLAG_EQ))
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_JEQ:
        if (tms9900.st & FLAG_EQ)
        {
            tms9900.pc += offset << 1;
        }

        break;

    case OP_SBZ:
        mprintf ("SBZ - bit fudged off\n");
        break;

    case OP_SBO:
        mprintf ("SBO - bit fudged on\n");
        break;

    case OP_TB:
        mprintf ("TB - fudging\n");
        tms9900.st &= ~FLAG_EQ;
        break;

    /*
     *
     */

    case OP_LDCR:
        mprintf ("LDCR - fudging\n");
        break;

    case OP_STCR:
        mprintf ("STCR - fudging\n");
        break;

    case OP_SRA:
        i32 = REG (sReg);
        i32 <<= 16;
        i32 >>= count;

        cpuWrite (AREG (sReg), i32 >> 16);
        compare (REG (sReg), 0);

        if (i32 & 0x8000)
        {
            carry = 1;
        }

        break;

    case OP_SRC:
        mprintf ("SRC : fudging\n");

    case OP_SRL:
        u32 = REG (sReg);
        u32 <<= 16;
        u32 >>= count;

        cpuWrite (AREG (sReg), u32 >> 16);
        compare (REG (sReg), 0);

        if (u32 & 0x8000)
        {
            carry = 1;
        }

        break;

    case OP_SLA:
        u32 = REG (sReg);
        u32 <<= count;

        cpuWrite (AREG (sReg), u32 & 0xFFFF);
        compare (REG (sReg), 0);

        /*
         *  Set carry flag on MSB set
         */

        if (u32 & 0x10000)
        {
            carry = 1;
        }

        break;

    case OP_BLWP:
        blwp (sAddr);
        break;

	case OP_B:
        mprintf ("Branch: %04X\n", sAddr);
        tms9900.pc = sAddr;
		break;

	case OP_X:
        mprintf ("X : fudged\n");
		break;

	case OP_CLR:
		cpuWrite (sAddr, 0);
		break;

	case OP_NEG:
		cpuWrite (sAddr, -sData);
		break;

	case OP_INV:
		cpuWrite (sAddr, ~sData);
        break;

    case OP_INC:
        cpuWrite (sAddr, sData+=1);
        compare (sData, 0);
        break;

    case OP_INCT:
        cpuWrite (sAddr, sData+=2);
        compare (sData, 0);
        break;

    case OP_DEC:
        cpuWrite (sAddr, sData-=1);
        compare (sData, 0);
        break;

    case OP_DECT:
        cpuWrite (sAddr, sData-=2);
        compare (sData, 0);
        break;

    case OP_BL:
		cpuWrite(AREG(11), tms9900.pc);
        tms9900.pc = sAddr;
		break;

    case OP_SWPB:
		cpuWrite (sAddr, SWAP(sData));
        break;

    case OP_SETO:
        cpuWrite (sAddr, 0xFFFF);
        break;

	case OP_ABS:
        if ((signed) sData < 0)
            cpuWrite (sAddr, -sData);

		break;
    default:
        halt ("Unknown opcode");
    }

    if (doStore)
    {
        if (isByte)
        {
            mprintf ("Result BYTE store: [%04X] = %02X\n",
                    (unsigned) dAddr, (unsigned) dData >> 8);
            cpuWriteB (dAddr, (WORD) (dData >> 8));
            compare (dData, 0);
            // compareB (dData, 0);
        }
        else
        {
            mprintf ("Result store: [%04X] = %04X\n",
                    (unsigned) dAddr, (unsigned) dData);
            cpuWrite (dAddr, dData);
            compare (dData, 0);
        }
    }

    if (carry)
    {
        tms9900.st |= FLAG_C;
    }
}

void showCPUStatus(void)
{
    WORD i;

    printf ("CPU\n");
    printf ("===\n");
    printf ("st=%04X\nwp=%04X\npc=%04X\n", tms9900.st, tms9900.wp, tms9900.pc);

    for (i = 0; i < 16; i++)
    {
        printf ("R%02d: %04X ", i, REG(i));
        if ((i + 1) % 4 == 0)
            printf ("\n");
    }
}

void showScratchPad (void)
{
    WORD i, j;

    printf ("Scratchpad\n");
    printf ("==========");

    for (i = 0; i < 256; i += 16 )
    {
        printf ("\n%04X - ", i + 0x8300);

        for (j = i; j < i + 16; j += 2)
        {
            printf ("%04X ",
                    cpuRead ((WORD)(j+0x8300)));
        }
    }

    printf ("\n");
}

void showGromStatus (void)
{
    printf ("GROM\n");
    printf ("====\n");

    printf ("addr        : %04X\n", gRom.addr);
    printf ("half-ad-set : %d\n", gRom.lowByteSet);
    printf ("half-ad-get : %d\n", gRom.lowByteGet);
}

static void boot (void)
{
    blwp (0x0);  // BLWP @>0
}

static void loadRom (void)
{
    FILE *fp;
    WORD addr, data;
    char s[80];

    if ((fp = fopen ("../rom.hex", "r")) == NULL)
    {
        printf ("can't open hex\n");
        exit (1);
    }

    while (!feof (fp))
    {
        if (fgets (s, sizeof s, fp))
        {
            if (sscanf (s, "%x %x\n", &addr, &data) == 2)
            {
                cpuWrite ((WORD) addr, (WORD) data);
            }
            else
            {
                printf ("can't parse '%s'\n", s);
            }
        }
    }

    fclose (fp);
    printf ("load ok\n");
}

static void loadGRom (void)
{
    FILE *fp;
    WORD addr;
    WORD data1, data2, data3, data4, data5, data6, data7, data8;
    char s[80];

    if ((fp = fopen ("../grom.hex", "r")) == NULL)
    {
        printf ("can't open grom.hex\n");
        exit (1);
    }

    while (!feof (fp))
    {
        if (fgets (s, sizeof s, fp))
        {
            if (sscanf (s, "%x %x %x %x %x %x %x %x %x\n", &addr,
                &data1,
                &data2,
                &data3,
                &data4,
                &data5,
                &data6,
                &data7,
                &data8) == 9)
            {
                gRom.rom.w[(addr>>1)  ] = SWAP(data1);
                gRom.rom.w[(addr>>1)+1] = SWAP(data2);
                gRom.rom.w[(addr>>1)+2] = SWAP(data3);
                gRom.rom.w[(addr>>1)+3] = SWAP(data4);
                gRom.rom.w[(addr>>1)+4] = SWAP(data5);
                gRom.rom.w[(addr>>1)+5] = SWAP(data6);
                gRom.rom.w[(addr>>1)+6] = SWAP(data7);
                gRom.rom.w[(addr>>1)+7] = SWAP(data8);

                if (addr < 0x30)
                {
                    printf ("GROM %04X = %02x %02x\n",
                            (unsigned) addr,
                            (unsigned) gRom.rom.b[addr],
                            (unsigned) gRom.rom.b[addr+1]);

                    printf ("input = %04X %04x %04x\n",
                            (unsigned) addr,
                            (unsigned) data1,
                            (unsigned) data2);
                }
            }
            else
            {
                printf ("can't parse '%s'\n", s);
            }
        }
    }

    fclose (fp);
    printf ("load ok\n");
}

static void input (void)
{
    char in[80];
    BOOL run = 0;
    WORD reg, data;
    long execCount = 0;
    // BOOL cover = 0;

    printf ("\nTMS9900 > ");
    fgets (in, sizeof in, stdin);
    printf ("\n");

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
        cpuWrite (AREG(reg), (WORD) data);

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
        quiet = !quiet;
        break;
    case 'q':
        exit (0);
        break;

    case 'v':
        vdpInitGraphics();
        break;
    }

    if (run)
    {
        while (!breakPointHit (tms9900.pc) &&
              // (!cover || tms9900.ram.covered[tms9900.pc>>1] == 1))
              1)
        {
            // tms9900.ram.covered[tms9900.pc>>1] = 1;
            execute (fetch());

            if ((execCount++ % 100) == 0)
            {
                vdpRefresh(0);

                if (kbhit ())
                   break;
            }

            watchShow();
            if (!quiet)
              showGromStatus();
        }
    }
}

int main (void) // int argc, char *argv[])
{
    loadRom ();
    loadGRom ();
    boot ();

    while (1)
    {
        input ();
    }

    return 0;

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
