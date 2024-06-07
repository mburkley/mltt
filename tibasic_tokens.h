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
 *  Using https://www.unige.ch/medecine/nouspikel/ti99/basic.htm
 */

#ifndef __TIBASIC_TOKENS_H
#define __TIBASIC_TOKENS_H

#include "types.h"

#define PROGRAM_TOP             0x37d7

/*  Some of the tokens have #defines to make the parser code easier to read.
 *  All others just have hardcoded values in the tokens table */
#define TOKEN_QUOTED_STRING     0xc7
#define TOKEN_UNQUOTED_STRING   0xc8
#define TOKEN_LINE_NUMBER       0xc9
#define TOKEN_ELSE              0x81
#define TOKEN_DOUBLE_COLON      0x82
#define TOKEN_EXCLAMATION       0x83
#define TOKEN_GO                0x85
#define TOKEN_GOTO              0x86
#define TOKEN_GOSUB             0x87
#define TOKEN_CALL              0x9d
#define TOKEN_THEN              0xb0
#define TOKEN_TO                0xb1

/*  These are pseudo-tokens used internally by the parser */
#define TOKEN_EOL               0x100
#define TOKEN_LITERAL           0x101
#define TOKEN_NUMBER            0x102

typedef struct _lineNumberTable
{
    uint16_t line;
    uint16_t address;
}
LineNumberTable;

typedef struct _fileHeader
{
    int16_t xorCheck;
    uint16_t lineNumbersTop;
    uint16_t lineNumbersBottom;
    uint16_t programTop;
} FileHeader;

typedef struct _Token
{
    uint8_t byte;
    char *token;
}
Token;
static Token tokens[] =
{
    { 0x00, "RUN" },
    { 0x01, "NEW" },
    { 0x02, "CONTINUE" },
    { 0x03, "LIST" },
    { 0x04, "BYE" },
    { 0x05, "NUMBER" },
    { 0x06, "OLD" },
    { 0x07, "RESEQUENCE" },
    { 0x08, "SAVE" },
    { 0x09, "EDIT" },
    { TOKEN_ELSE, "ELSE" },
    { TOKEN_DOUBLE_COLON, "::" },
    { TOKEN_EXCLAMATION, "!" },
    { 0x84, "IF" },
    { TOKEN_GO, "GO" },
    { TOKEN_GOTO, "GOTO" },
    { TOKEN_GOSUB, "GOSUB" },
    { 0x88, "RETURN" },
    { 0x89, "DEF" },
    { 0x8A, "DIM" },
    { 0x8B, "END" },
    { 0x8C, "FOR" },
    { 0x8D, "LET" },
    { 0x8E, "BREAK" },
    { 0x8F, "UNBREAK" },
    { 0x90, "TRACE" },
    { 0x91, "UNTRACE" },
    { 0x92, "INPUT" },
    { 0x93, "DATA" },
    { 0x94, "RESTORE" },
    { 0x95, "RANDOMIZE" },
    { 0x96, "NEXT" },
    { 0x97, "READ" },
    { 0x98, "STOP" },
    { 0x99, "DELETE" },
    { 0x9A, "REM" },
    { 0x9B, "ON" },
    { 0x9C, "PRINT" },
    { TOKEN_CALL, "CALL" },
    { 0x9E, "OPTION" },
    { 0x9F, "OPEN" },
    { 0xA0, "CLOSE" },
    { 0xA1, "SUB" },
    { 0xA2, "DISPLAY" },
    { 0xA4, "ACCEPT" },
    { 0xA6, "WARNING" },
    { 0xA7, "SUBEXIT" },
    { 0xA8, "SUBEND" },
    { 0xA9, "RUN" },
    { TOKEN_THEN, "THEN" },
    { TOKEN_TO, "TO" },
    { 0xB2, "STEP" },
    { 0xB3, "," },
    { 0xB4, ";" },
    { 0xB5, ":" },
    { 0xB6, ")" },
    { 0xB7, "(" },
    { 0xB8, "&" },
    { 0xBA, "OR" },
    { 0xBB, "AND" },
    { 0xBD, "NOT" },
    { 0xBE, "=" },
    { 0xBF, "<" },
    { 0xC0, ">" },
    { 0xC1, "+" },
    { 0xC2, "-" },
    { 0xC3, "*" },
    { 0xC4, "/" },
    { 0xC5, "^" },
    { 0xCA, "EOF" },
    { 0xCB, "ABS" },
    { 0xCC, "ATN" },
    { 0xCD, "COS" },
    { 0xCE, "EXP" },
    { 0xCF, "INT" },
    { 0xD0, "LOG" },
    { 0xD1, "SGN" },
    { 0xD2, "SIN" },
    { 0xD3, "SQR" },
    { 0xD4, "TAN" },
    { 0xD5, "LEN" },
    { 0xD6, "CHR$" },
    { 0xD7, "RND" },
    { 0xD8, "SEG$" },
    { 0xD9, "POS" },
    { 0xDA, "VAL" },
    { 0xDB, "STR$" },
    { 0xDC, "ASC" },
    { 0xDE, "REC" },
    { 0xE0, "MIN" },
    { 0xE1, "RPT$" },
    { 0xE8, "NUMERIC" },
    { 0xE9, "DIGIT" },
    { 0xEB, "SIZE" },
    { 0xEC, "ALL " },
    { 0xED, "USING" },
    { 0xEE, "BEEP" },
    { 0xEF, "ERASE" },
    { 0xF0, "AT" },
    { 0xF1, "BASE" },
    { 0xF3, "VARIABLE" },
    { 0xF4, "RELATIVE" },
    { 0xF5, "INTERNAL" },
    { 0xF6, "SEQUENTIAL" },
    { 0xF7, "OUTPUT" },
    { 0xF8, "UPDATE" },
    { 0xF9, "APPEND" },
    { 0xFA, "FIXED" },
    { 0xFB, "PERMANENT" },
    { 0xFC, "TAB" },
    { 0xFD, "#" },
    { 0xFE, "VALIDATE" }
};

#define NUM_TOKENS (sizeof (tokens) / sizeof (Token))

#endif

