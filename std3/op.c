#define OPTYPE_IMMED    1
#define OPTYPE_SINGLE   2
#define OPTYPE_SHIFT    3
#define OPTYPE_JUMP     4
#define OPTYPE_DUAL1    5
#define OPTYPE_DUAL2    6

struct
{
    WORD index;
    WORD type;
    WORD opMask;
    BOOL store;
    BOOL hasDest;
    BOOL cmpZero;
}
opGroup[64] =
{
    { 0x00, OPTYPE_IMMED,   0xFFE0, 1, 0, 1 },
    { 0x01, OPTYPE_SINGLE,  0xFFC0, 1, 0, 1 },
    { 0x02, OPTYPE_SHIFT,   0xFF00, 1, 0, 1 },
    { 0x03, 0,              0xFFFF, 0, 0, 0 },
    { 0x04, OPTYPE_JUMP,    0xFF00, 0, 0, 0 },
    { 0x05, OPTYPE_JUMP,    0xFF00, 0, 0, 0 },
    { 0x06, OPTYPE_JUMP,    0xFF00, 0, 0, 0 },
    { 0x07, OPTYPE_JUMP,    0xFF00, 0, 0, 0 },
    { 0x08, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x09, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x0A, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x0B, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x0C, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x0D, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x0E, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x0F, OPTYPE_DUAL1,   0xFC00, 1, 1, 1 },
    { 0x10, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x11, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x12, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x13, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x14, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x15, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x16, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x17, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x18, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x19, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x1A, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x1B, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x1C, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x1D, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x1E, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x1F, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x20, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x21, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x22, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x23, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x24, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x25, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x26, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x27, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x28, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x29, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x2A, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x2B, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x2C, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x2D, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x2E, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x2F, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x30, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x31, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x32, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x33, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x34, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x35, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x36, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x37, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x38, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x39, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x3A, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x3B, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x3C, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x3D, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x3E, OPTYPE_DUAL2,   0xF000, 1, 1, 1 },
    { 0x3F, OPTYPE_DUAL2,   0xF000, 1, 1, 1 }
};

