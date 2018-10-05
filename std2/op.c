struct
{
    WORD opCode;
    WORD opMask;
    BOOL sReg;
    BOOL dReg;
    BOOL sMode;
    BOOL dMode;
    BOOL offset;
    BOOL store;
    BOOL cmpZero;

opGroup[64] =
{
    0x00, OP_LI, 0xFFF0, 1, 0,

OP_LI   0x0200
OP_AI   0x0220
OP_ANDI 0x0240
OP_ORI  0x0260
OP_CI   0x0280
OP_STWP 0x02A0
OP_STST 0x02C0
OP_LWPI 0x02E0
OP_LIMI 0x0300

    0x01, OP_BLWP

OP_BLWP 0x0400
OP_B    0x0440
OP_X    0x0480
OP_CLR  0x04C0
OP_NEG  0x0500
OP_INV  0x0540
OP_INC  0x0580
OP_INCT 0x05C0
OP_DEC  0x0600
OP_DECT 0x0640
OP_BL   0x0680
OP_SWPB 0x06C0
OP_SETO 0x0700
OP_ABS  0x0740

   0x02, OP_SRA

OP_SRA  0x0800
OP_SRL  0x0900
OP_SLA  0x0A00
OP_SRC  0x0B00

    0x03, invalid
    0x04, OP_JMP

OP_JMP  0x1000
OP_JLT  0x1100
OP_JLE  0x1200
OP_JEQ  0x1300
    0x05, OP_JHE
OP_JHE  0x1400
OP_JGT  0x1500
OP_JNE  0x1600
OP_JNC  0x1700
    0x06, OP_JOC
OP_JOC  0x1800
OP_JNO  0x1900
OP_JL   0x1A00
OP_JH   0x1B00
    0x07, OP_JOP
OP_JOP  0x1C00
OP_SBO  0x1D00
OP_SBZ  0x1E00
OP_TB   0x1F00

    8, OP_COC
OP_COC  0x2000
    9
OP_CZC  0x2400
    A
OP_XOR  0x2800
    B
OP_XOP  0x2C00
    C
OP_LDCR 0x3000
    D
OP_STCR 0x3400
    E
OP_MPY  0x3800
    F
OP_DIV  0x3C00
    10
    11
    12
    13
    14
    15
    16
    17
OP_SZC  0x4000
    18
    19
    1A
    1B
    1C
    1D
    1E
    1F
OP_S    0x6000
    20
    21
    22
    23
    24
    25
    26
    27
OP_C    0x8000
    28
    29
    2A
    2B
    2C
    2D
    2E
    2F
OP_A    0xA000
    30
    31
    32
    33
    34
    35
    36
    37
OP_MOV  0xC000
    38
    39
    3A
    3B
    3C
    3D
    3E
    3F
OP_SOC  0xE000

