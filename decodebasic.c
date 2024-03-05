
/*
 *  Using https://www.unige.ch/medecine/nouspikel/ti99/basic.htm
 */

#include <stdio.h>
#include "decodebasic.h"

static int decodeQuotedString (uint8_t *data)
{
    int strlen = *data++;

    // printf("stlen=%d\n", strlen);
    printf ("\"%-*.*s\"", strlen, strlen, data);

    return strlen+1;
}

static int decodeUnquotedString (uint8_t *data)
{
    int strlen = *data++;

    // printf("stlen=%d\n", strlen);
    printf ("%-*.*s", strlen, strlen, data);

    return strlen+1;
}

static int decodeLineNumber (uint8_t *data, int len)
{
    int line = 0;
    int bytes = 0;

    len--;
    while (len--)
    {
        line = line * 256 + *data++;
        bytes++;
    }

    printf ("%d", line);
    return bytes;
}

static void decodeLine (uint8_t *data)
{
    int lineLen = *data++ - 1;
    // printf ("Line len %d (%x)\n", lineLen, lineLen);
    if (!lineLen)
    return;

    while (lineLen>0)
    {
        int stlen = 1;
        // printf (",%02X:", *data);
        if (*data < 0x80)
            printf ("%c", *data);
        else
        switch (*data)
        {
        case 0x00: printf ("RUN"); break;
        case 0x01: printf ("NEW"); break;
        case 0x02: printf ("CONTINUE"); break;
        case 0x03: printf ("LIST"); break;
        case 0x04: printf ("BYE"); break;
        case 0x05: printf ("NUMBER"); break;
        case 0x06: printf ("OLD"); break;
        case 0x07: printf ("RESEQUENCE"); break;
        case 0x08: printf ("SAVE"); break;
        case 0x09: printf ("EDIT"); break;
        case 0x81: printf ("ELSE "); break;
        case 0x82: printf (" :: "); break;
        case 0x83: printf ("!"); break;
        case 0x84: printf ("IF "); break;
        case 0x85: printf ("GO "); break;
        case 0x86: printf ("GOTO "); break;
        case 0x87: printf ("GOSUB "); break;
        case 0x88: printf ("RETURN"); break;
        case 0x89: printf ("DEF "); break;
        case 0x8A: printf ("DIM "); break;
        case 0x8B: printf ("END"); break;
        case 0x8C: printf ("FOR "); break;
        case 0x8D: printf ("LET "); break;
        case 0x8E: printf ("BREAK"); break;
        case 0x8F: printf ("UNBREAK"); break;
        case 0x90: printf ("TRACE"); break;
        case 0x91: printf ("UNTRACE"); break;
        case 0x92: printf ("INPUT "); break;
        case 0x93: printf ("DATA "); break;
        case 0x94: printf ("RESTORE"); break;
        case 0x95: printf ("RANDOMIZE"); break;
        case 0x96: printf ("NEXT "); break;
        case 0x97: printf ("READ "); break;
        case 0x98: printf ("STOP"); break;
        case 0x99: printf ("DELETE"); break;
        case 0x9A: printf ("REM "); break;
        case 0x9B: printf ("ON "); break;
        case 0x9C: printf ("PRINT "); break;
        case 0x9D: printf ("CALL "); break;
        case 0x9E: printf ("OPTION "); break;
        case 0x9F: printf ("OPEN "); break;
        case 0xA0: printf ("CLOSE "); break;
        case 0xA1: printf ("SUB "); break;
        case 0xA2: printf ("DISPLAY "); break;
        case 0xA4: printf ("ACCEPT "); break;
        case 0xA6: printf ("WARNING "); break;
        case 0xA7: printf ("SUBEXIT"); break;
        case 0xA8: printf ("SUBEND"); break;
        case 0xA9: printf ("RUN "); break;
        case 0xB0: printf (" THEN "); break;
        case 0xB1: printf (" TO "); break;
        case 0xB2: printf ("STEP "); break;
        case 0xB3: printf (","); break;
        case 0xB4: printf (";"); break;
        case 0xB5: printf (":"); break;
        case 0xB6: printf (")"); break;
        case 0xB7: printf ("("); break;
        case 0xB8: printf ("&"); break;
        case 0xBA: printf (" OR "); break;
        case 0xBB: printf (" AND "); break;
        case 0xBD: printf (" NOT "); break;
        case 0xBE: printf ("="); break;
        case 0xBF: printf ("<"); break;
        case 0xC0: printf (">"); break;
        case 0xC1: printf ("+"); break;
        case 0xC2: printf ("-"); break;
        case 0xC3: printf ("*"); break;
        case 0xC4: printf ("/"); break;
        case 0xC5: printf ("^"); break;
        case 0xC7: stlen += decodeQuotedString (data+1); break;
        case 0xC8: stlen += decodeUnquotedString (data+1); break;
        case 0xC9: stlen += decodeLineNumber (data+1, lineLen); break;
        case 0xCA: printf ("EOF"); break;
        case 0xCB: printf ("ABS"); break;
        case 0xCC: printf ("ATN"); break;
        case 0xCD: printf ("COS"); break;
        case 0xCE: printf ("EXP"); break;
        case 0xCF: printf ("INT"); break;
        case 0xD0: printf ("LOG"); break;
        case 0xD1: printf ("SGN"); break;
        case 0xD2: printf ("SIN"); break;
        case 0xD3: printf ("SQR"); break;
        case 0xD4: printf ("TAN"); break;
        case 0xD5: printf ("LEN"); break;
        case 0xD6: printf ("CHR$"); break;
        case 0xD7: printf ("RND"); break;
        case 0xD8: printf ("SEG$"); break;
        case 0xD9: printf ("POS"); break;
        case 0xDA: printf ("VAL"); break;
        case 0xDB: printf ("STR$"); break;
        case 0xDC: printf ("ASC"); break;
        case 0xDE: printf ("REC"); break;
        case 0xE0: printf ("MIN"); break;
        case 0xE1: printf ("RPT$"); break;
        case 0xE8: printf ("NUMERIC"); break;
        case 0xE9: printf ("DIGIT"); break;
        case 0xEB: printf ("SIZE"); break;
        case 0xEC: printf ("ALL "); break;
        case 0xED: printf (" USING "); break;
        case 0xEE: printf ("BEEP "); break;
        case 0xEF: printf ("ERASE "); break;
        case 0xF0: printf ("AT"); break;
        case 0xF1: printf ("BASE"); break;
        case 0xF3: printf ("VARIABLE"); break;
        case 0xF4: printf ("RELATIVE"); break;
        case 0xF5: printf ("INTERNAL"); break;
        case 0xF6: printf ("SEQUENTIAL"); break;
        case 0xF7: printf ("OUTPUT"); break;
        case 0xF8: printf ("UPDATE"); break;
        case 0xF9: printf ("APPEND"); break;
        case 0xFA: printf ("FIXED"); break;
        case 0xFB: printf ("PERMANENT"); break;
        case 0xFC: printf ("TAB"); break;
        case 0xFD: printf ("#"); break;
        case 0xFE: printf ("VALIDATE "); break;
        default: printf ("[??? %02X]", *data); break;
        }
        data += stlen;
        // len -= stlen;
        lineLen -= stlen;
    }

    printf ("\n");
    // printf ("EOL, rem=%d\n", len);
}

void decodeBasicProgram (uint8_t *data, int len)
{
    int xorCheck = WORD (data[0], data[1]);
    int addrLineNumbersTop = WORD (data[2], data[3]);
    int addrLineNumbersEnd = WORD (data[4], data[5]);
    // int addrProgramTop = WORD (data[6], data[7]);
    // printf ("\ncodeLen = %04X\n", xorCheck);
    if (xorCheck < 0)
    {
        fprintf (stderr, "** Protected\n\n");
        xorCheck = -xorCheck;
    }
    xorCheck -= (addrLineNumbersTop ^ addrLineNumbersEnd);
    if (xorCheck != 0)
    {
        fprintf (stderr, "** Checksum invalid\n\n");
    }
    // printf ("top of line numbers = %04X\n", addrLineNumbersTop);
    // printf ("addrLineNumbersEnd = %04X\n", addrLineNumbersEnd);
    // printf ("program top = %04X\n", addrProgramTop);

    // printf ("line number table size %04X\n", addrLineNumbersTop - addrLineNumbersEnd);
    int lineCount = (addrLineNumbersTop - addrLineNumbersEnd + 1) / 4;
    // int codeOffset = addrProgramTop - addrLineNumbersTop;
    // printf ("code offset=%04X\n", codeOffset);
    data += 8;
    len -= 8;

    // printf ("prog len=%d\n", len);
    /*  Decode line number table */
    for (int i = lineCount - 1; i >= 0; i--)
    {
        int line = WORD (data[i*4], data[i*4+1]);
        int address = WORD (data[i*4+2], data[i*4+3]);
        // printf ("L%d %04X\n", line, address);
        printf ("%d ", line);
        address = address - addrLineNumbersEnd - 1;
        // addrProgramTop - address + addrLineNumbersTop - addrLineNumbersEnd;

        // printf ("decode %04X(%d)\n", address, address);
        decodeLine (&data[address]);
        // if (*data > 0x7F)
        //     break;

        // printf ("L%4d %04X\n", WORD (data[0], data[1]), WORD (data[2], data[3]));
        // data += 4;
        // len -= 4;
        // lineCount--;
    }

}


