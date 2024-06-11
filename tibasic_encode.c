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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "tibasic.h"
#include "tibasic_tokens.h"

#define MAX_LINE_NUMBER 32767

static int currToken;
static char currTokenValue[256];
static uint8_t *outp;
static char *inp;
static LineNumberTable lineTable[MAX_LINE_NUMBER];

static void emitString (int type)
{
    int len = strlen (currTokenValue);

    if (type != TOKEN_LITERAL)
    {
        *outp++ = type;
        *outp++ = len;
    }

    memcpy (outp, currTokenValue, len);
    outp += len;
}

static void emitLineNumber (void)
{
    int line = atoi (currTokenValue);
    *outp++ = TOKEN_LINE_NUMBER;
    *outp++ = line >> 8;
    *outp++ = (line & 0xff);
}

static void emitToken (int token)
{
    /*  Numbers should be encoded as line numbers for GOTO, THEN, ELSE, etc */
    if (token == TOKEN_LINE_NUMBER && currToken == TOKEN_NUMBER)
    {
        emitLineNumber ();
        return;
    }

    /*  Strings following CALL should be emitted as unquoted strings */
    if (token == TOKEN_UNQUOTED_STRING && currToken == TOKEN_LITERAL)
    {
        emitString (token);
        return;
    }

    if (currToken != token)
    {
        printf ("Expected token %02x but have token %02x\n", token,
                 currToken);
        return;
    }

    if (token == TOKEN_EOL)
    {
        *outp++ = 0;
        return;
    }

    /*  A number that is not a line number is emitted as an unquoted string */
    if (token == TOKEN_NUMBER)
    {
        emitString (TOKEN_UNQUOTED_STRING);
        return;
    }

    if (token == TOKEN_QUOTED_STRING || 
        token == TOKEN_UNQUOTED_STRING || 
        token == TOKEN_LITERAL)
    {
        emitString (currToken);
        return;
    }

    /*  Default emit the actual token value into the output stream */
    *outp++ = currToken;
}


static int findToken (const char *token, int len)
{
    for (int i = 0; i < NUM_TOKENS; i++)
        if (strlen (tokens[i].token) == len &&
            !strncmp (tokens[i].token, token, len))
        {
            return tokens[i].byte;
        }

    return -1;
}

static void parseQuotedString (void)
{
    inp++; // skip the opening double quote
    char *end = inp;

    while (*end && *end != '"' && *end != '\n')
        end++;

    int len = end - inp;

    strncpy (currTokenValue, inp, len);
    currTokenValue[len] = 0;
    currToken = TOKEN_QUOTED_STRING;
    inp = end;
    inp++; // skip the closing quote
}

/*  An identifier is a sequence of at least one characters.  It may be an
 *  operator or a command like GOSUB or a variable name.  It may end in a $.
 *  When the end of the identifier is found an attempt is made to match it
 *  against known tokens.  If it is found, that token is emitted, otherwise it
 *  is emitted as a literal unless we were asked to emit an unquoted string */
static void parseIdentifier (bool debug)
{
    char *end = inp;

    if (isalpha (*end))
    {
        do
        {
            /*  $ can only be the last character of an identifier so break if we
             *  find one */
            if (*end++ == '$')
                break;
        }
        while (isalpha (*end) || *end == '$');
    }
    else
        end++; // get a single character
            
    int len = end - inp;

    if ((currToken = findToken (inp, len)) != -1)
    {
        if (debug) printf ("matched token '%-*.*s'=%02x\n", len, len, inp, currToken);
        inp = end;
        return;
    }

    if (debug) printf ("literal '%-*.*s'\n", len, len, inp);
    strncpy (currTokenValue, inp, len);
    currTokenValue[len] = 0;
    currToken = TOKEN_LITERAL;
    inp = end;
}

static void parseNumber (bool debug)
{
    char *end = inp;

    while (isdigit (*end) || *end == 'E')
    {
        if (debug) printf ("parse digit %c\n", *end);

        if (*end++ == 'E')
        {
            if (*end == '+' || *end == '-')
                end++;
        }
    }

    int len = end - inp;

    if (debug) printf ("number \"%-*.*s\"\n", len, len, inp);
    strncpy (currTokenValue, inp, len);
    currTokenValue[len] = 0;
    currToken = TOKEN_NUMBER;
    inp = end;
}

/*  Parse the next token from the input stream */
static void parseToken (bool debug)
{
    do
    {
        // Get the next token
        if (*inp == '\n')
        {
            if (debug) printf ("eol\n");
            inp++;
            currToken = TOKEN_EOL;
            break;
        }
        else if (*inp == ' ')
        {
            if (debug) printf ("skip space\n");
            inp++;
            continue;
        }
        #ifdef EXTENDED_BASIC
        else if (inp[0] == ':' && inp[1] == ':')
        {
            if (debug) printf ("double colon\n");
            currToken = TOKEN_DOUBLE_COLON;
            inp += 2;
            break;
        }
        #endif
        else if (*inp == '"')
        {
            if (debug) printf ("quoted string\n");
            parseQuotedString ();
            break;
        }
        else if (isdigit (*inp))
        {
            if (debug) printf ("number\n");
            parseNumber (debug);
            break;
        }
        else
        {
            if (debug) printf ("ident\n");
            parseIdentifier (debug);
            break;
        }
    }
    while (1);
}

/*  Verifies the current token against what is expected and outputs the token to
 *  the output stream if correct */
static void acceptToken (int token, bool debug)
{
    if (debug) printf ("accept %02x\n", token);
    parseToken (debug);
}

static void processLine (bool debug)
{
    acceptToken (TOKEN_LINE_NUMBER, debug);

    while (currToken != TOKEN_EOL)
    {
        switch (currToken)
        {
        case TOKEN_GO:
            emitToken (TOKEN_GO);
            acceptToken (TOKEN_GO, debug);
            if (currToken == TOKEN_TO)
            {
                emitToken (TOKEN_TO);
                acceptToken (TOKEN_TO, debug);
            }
            emitToken (TOKEN_LINE_NUMBER);
            acceptToken (TOKEN_LINE_NUMBER, debug);
            break;
        case TOKEN_GOTO:
        case TOKEN_GOSUB:
        case TOKEN_THEN:
        case TOKEN_ELSE:
            emitToken (currToken);
            acceptToken (currToken, debug);
            emitToken (TOKEN_LINE_NUMBER);
            acceptToken (TOKEN_LINE_NUMBER, debug);
            break;
        case TOKEN_CALL:
            emitToken (TOKEN_CALL);
            acceptToken (TOKEN_CALL, debug);
            emitToken (TOKEN_UNQUOTED_STRING);
            acceptToken (TOKEN_UNQUOTED_STRING, debug);
            break;
        default:
            emitToken (currToken);
            acceptToken (currToken, debug);
            break;
        }
    }

    emitToken (TOKEN_EOL);
    acceptToken (TOKEN_EOL, debug);
}

int encodeBasicProgram (char *input, int inputLen, uint8_t **output, bool debug)
{
    int lineCount = 0;

    /*  Allocate a buffer to receive the tokenised output which is 50% bigger
     *  than the source code input */
    if ((*output = realloc (*output, inputLen * 1.5)) == NULL)
    {
        fprintf (stderr, "Can't allocate buffer for encoded basic\n");
        exit (1);
    }

    inp = input;
    outp = *output;
    FileHeader *header = (FileHeader*) *output;
    acceptToken (0, debug); // Fetch the first token

    while (inp - input < inputLen)
    {
        uint8_t *lineStart = outp++;
        if (debug) printf ("parse %ld/%d\n", inp - input, inputLen);

        int line = atoi (currTokenValue);

        if (line == 0)
            break;

        lineTable[lineCount].line = line;
        lineTable[lineCount].address = outp - *output;
        lineCount++;
        processLine (debug);
        *lineStart = outp - lineStart - 1; // tokenised line length
    }
    if (debug) printf ("done\n");

    int lenHeader = sizeof (FileHeader) + sizeof (LineNumberTable) * lineCount;
    int lenCode = outp - *output;

    memmove (*output + lenHeader,
             *output,
             lenCode);

    /*  Convert header and line numbers into be16 in reverse order */
    LineNumberTable *outputLineTable = (LineNumberTable*) (*output + sizeof (FileHeader));
    for (int i = lineCount-1; i >= 0; i--)
    {
        outputLineTable->line = htobe16 (lineTable[i].line);
        outputLineTable->address = htobe16 (PROGRAM_TOP - lenCode + lineTable[i].address + 1);
        outputLineTable++;
    }

    header->programTop = htobe16 (PROGRAM_TOP);
    header->lineNumbersTop = htobe16 (PROGRAM_TOP - lenCode);
    header->lineNumbersBottom = htobe16 (PROGRAM_TOP - lenCode - lineCount * sizeof (LineNumberTable) + 1);
    header->xorCheck = (header->lineNumbersTop ^ header->lineNumbersBottom);

    return lenHeader + lenCode;
}

