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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "tibasic.h"
#include "tibasic_tokens.h"

static char *outPrintf (char *output, const char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    int count = vsprintf (output, fmt, ap);
    va_end (ap);

    if (count < 0)
    {
        fprintf (stderr, "output string '%s' failed\n", fmt);
        return output;
    }

    return output + count;
}

static int decodeQuotedString (char **output, uint8_t *data)
{
    int strlen = *data++;

    *output = outPrintf (*output, "\"%-*.*s\"", strlen, strlen, data);

    return strlen+1;
}

static int decodeUnquotedString (char **output, uint8_t *data)
{
    int strlen = *data++;

    *output = outPrintf (*output, "%-*.*s", strlen, strlen, data);

    return strlen+1;
}

static int decodeLineNumber (char **output, uint8_t *data)
{
    int line = (data[0] << 8) + data[1];
    *output = outPrintf (*output, "%d", line);
    return 2;
}

static void decodeLine (char **output, uint8_t *data, bool debug)
{
    int lineLen = *data++ - 1;
    int prevToken = TOKEN_EOL;

    if (debug) *output += sprintf (*output, "line-len=%d\n", lineLen);

    if (!lineLen)
        return;

    while (lineLen > 0)
    {
        int stlen = 1;
        if (*data < 0x80)
        {
            *output += sprintf (*output, "%c", *data);
        }
        else if (*data == TOKEN_QUOTED_STRING)
        {
            if (debug) *output += sprintf (*output, "[");
            stlen += decodeQuotedString (output, data+1);
            if (debug) *output += sprintf (*output, "]");
        }
        else if (*data == TOKEN_UNQUOTED_STRING)
        {
            if (debug) *output += sprintf (*output, "['");
            stlen += decodeUnquotedString (output, data+1);
            if (debug) *output += sprintf (*output, "']");
        }
        else if (*data == TOKEN_LINE_NUMBER)
        {
            if (debug) *output += sprintf (*output, "[#");
            stlen += decodeLineNumber (output, data+1);
            if (debug) *output += sprintf (*output, "]");
        }
        else
        {
            unsigned i;
            for (i = 0; i < NUM_TOKENS; i++)
                if (tokens[i].byte == *data)
                {
                    if ((*data == TOKEN_COLON && prevToken == TOKEN_COLON) ||
                        ((tokens[i].space && prevToken != TOKEN_EOL) &&
                        (prevToken < 0x80 ||
                         prevToken == TOKEN_LINE_NUMBER ||
                         prevToken == TOKEN_UNQUOTED_STRING ||
                         prevToken == TOKEN_QUOTED_STRING)))
                    {
                             *(*output)++ = ' ';
                    }

                    if (debug) *output += sprintf (*output, "[%02x-", tokens[i].byte);
                    *output += sprintf (*output, "%s", tokens[i].token);
                    if (debug) *output += sprintf (*output, "]");

                    if (tokens[i].space)
                         *(*output)++ = ' ';
                    break;
                }

            if (i == NUM_TOKENS)
                *output += sprintf (*output, "[??? %02X]", *data);
        }

        prevToken = *data;
        data += stlen;
        lineLen -= stlen;
    }

    *(*output)++ = '\n';
}

/*  Verify that the input is a valid tokenised basic file by doing some simple
 *  sanity checks */
static bool decodeBasicFileCheck (FileHeader *header)
{
    header->xorCheck -= (header->lineNumbersTop ^ header->lineNumbersBottom);

    if (header->xorCheck != 0)
        return false;

    if (header->lineNumbersTop < header->lineNumbersBottom ||
        header->programTop < header->lineNumbersBottom ||
        header->programTop < header->lineNumbersTop ||
        header->programTop > 0x3fff)
        return false;

    return true;
}

int decodeBasicProgram (uint8_t *input, int inputLen, char *output, bool debug)
{
    FileHeader *header = (FileHeader*) input;

    char *outp = output;

    header->xorCheck = be16toh (header->xorCheck);
    header->lineNumbersTop = be16toh (header->lineNumbersTop);
    header->lineNumbersBottom = be16toh (header->lineNumbersBottom);
    header->programTop = be16toh (header->programTop);

    if (header->xorCheck < 0)
    {
        fprintf (stderr, "** Protected\n\n");
        header->xorCheck = -header->xorCheck;
    }
    
    if (!decodeBasicFileCheck (header))
    {
        fprintf (stderr,
                 "** Checksum invalid or header field out of range - this does not appear to be a basic program\n\n");
        return 0;
    }

    int lineCount = (header->lineNumbersTop - header->lineNumbersBottom + 1) / 4;

    if (debug)
    {
        outp = outPrintf (outp, "# top %04X\n", header->lineNumbersTop);
        outp = outPrintf (outp, "# bot %04X\n", header->lineNumbersBottom);
        outp = outPrintf (outp, "# prog %04X\n", header->programTop);
    }

    input += 8;
    inputLen -= 8;
    LineNumberTable *table = (LineNumberTable*) input;

    for (int i = lineCount - 1; i >= 0; i--)
    {
        int line = be16toh (table[i].line);

        if (debug)
            outp = outPrintf (outp, "# line %d is at address %04x\n", line, be16toh (table[i].address));

        int address = be16toh (table[i].address) - header->lineNumbersBottom - 1;
        outp = outPrintf (outp, "%d ", line);

        decodeLine (&outp, &input[address], debug);
    }

    if (debug)
    {
        printf ("# processed %ld out of %d space for output\n", outp-output,
        inputLen *2);
    }

    return outp - output;
}

