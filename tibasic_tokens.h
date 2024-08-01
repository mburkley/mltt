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
#define TOKEN_COLON             0xb5
#define TOKEN_THEN              0xb0
#define TOKEN_TO                0xb1

/*  These are pseudo-tokens used internally by the parser */
#define TOKEN_EOL               0x100
#define TOKEN_LITERAL           0x101
#define TOKEN_NUMBER            0x102
#define TOKEN_SINGLE            0x103
#define TOKEN_MULTI             0x104

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
    const char *token;
    bool space;
}
Token;
static Token tokens[] =
{
    { 0x00, "RUN", false},
    { 0x01, "NEW", false},
    { 0x02, "CONTINUE", false},
    { 0x03, "LIST", false},
    { 0x04, "BYE", false},
    { 0x05, "NUMBER", false},
    { 0x06, "OLD", false},
    { 0x07, "RESEQUENCE", false},
    { 0x08, "SAVE", false},
    { 0x09, "EDIT", false},
    { TOKEN_ELSE, "ELSE", true},
    { TOKEN_DOUBLE_COLON, "::", true},
    { TOKEN_EXCLAMATION, "!", false},
    { 0x84, "IF", true},
    { TOKEN_GO, "GO", true},
    { TOKEN_GOTO, "GOTO", true},
    { TOKEN_GOSUB, "GOSUB", true},
    { 0x88, "RETURN", true},
    { 0x89, "DEF", true},
    { 0x8A, "DIM", true},
    { 0x8B, "END", true},
    { 0x8C, "FOR", true},
    { 0x8D, "LET", true},
    { 0x8E, "BREAK", true},
    { 0x8F, "UNBREAK", true},
    { 0x90, "TRACE", true},
    { 0x91, "UNTRACE", true},
    { 0x92, "INPUT", true},
    { 0x93, "DATA", true},
    { 0x94, "RESTORE", true},
    { 0x95, "RANDOMIZE", true},
    { 0x96, "NEXT", true},
    { 0x97, "READ", true},
    { 0x98, "STOP", true},
    { 0x99, "DELETE", true},
    { 0x9A, "REM", false},
    { 0x9B, "ON", true},
    { 0x9C, "PRINT", true},
    { TOKEN_CALL, "CALL", true},
    { 0x9E, "OPTION", true},
    { 0x9F, "OPEN", true},
    { 0xA0, "CLOSE", true},
    { 0xA1, "SUB", true},
    { 0xA2, "DISPLAY", true},
    { 0xA4, "ACCEPT", true},
    { 0xA6, "WARNING", true},
    { 0xA7, "SUBEXIT", true},
    { 0xA8, "SUBEND", true},
    { 0xA9, "RUN", true},
    { TOKEN_THEN, "THEN", true},
    { TOKEN_TO, "TO", true},
    { 0xB2, "STEP", true},
    { 0xB3, ",", false},
    { 0xB4, ";", false},
    { TOKEN_COLON, ":", false},
    { 0xB6, ")", false},
    { 0xB7, "(", false},
    { 0xB8, "&", false},
    { 0xBA, "OR", true},
    { 0xBB, "AND", true},
    { 0xBD, "NOT", true},
    { 0xBE, "=", false},
    { 0xBF, "<", false},
    { 0xC0, ">", false},
    { 0xC1, "+", false},
    { 0xC2, "-", false},
    { 0xC3, "*", false},
    { 0xC4, "/", false},
    { 0xC5, "^", false},
    { 0xCA, "EOF", false},
    { 0xCB, "ABS", false},
    { 0xCC, "ATN", false},
    { 0xCD, "COS", false},
    { 0xCE, "EXP", false},
    { 0xCF, "INT", false},
    { 0xD0, "LOG", false},
    { 0xD1, "SGN", false},
    { 0xD2, "SIN", false},
    { 0xD3, "SQR", false},
    { 0xD4, "TAN", false},
    { 0xD5, "LEN", false},
    { 0xD6, "CHR$", false},
    { 0xD7, "RND", false},
    { 0xD8, "SEG$", false},
    { 0xD9, "POS", false},
    { 0xDA, "VAL", false},
    { 0xDB, "STR$", false},
    { 0xDC, "ASC", false},
    { 0xDE, "REC", false},
    { 0xE0, "MIN", false},
    { 0xE1, "RPT$", false},
    { 0xE8, "NUMERIC", false},
    { 0xE9, "DIGIT", false},
    { 0xEB, "SIZE", false},
    { 0xEC, "ALL", false},
    { 0xED, "USING", false},
    { 0xEE, "BEEP", false},
    { 0xEF, "ERASE", true},
    { 0xF0, "AT", false},
    { 0xF1, "BASE", false},
    { 0xF3, "VARIABLE", false},
    { 0xF4, "RELATIVE", false},
    { 0xF5, "INTERNAL", false},
    { 0xF6, "SEQUENTIAL", false},
    { 0xF7, "OUTPUT", false},
    { 0xF8, "UPDATE", false},
    { 0xF9, "APPEND", false},
    { 0xFA, "FIXED", false},
    { 0xFB, "PERMANENT", false},
    { 0xFC, "TAB", false},
    { 0xFD, "#", false},
    { 0xFE, "VALIDATE", false}
};

#define NUM_TOKENS (sizeof (tokens) / sizeof (Token))

#endif

