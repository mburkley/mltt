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
#include <string.h>

#include "textgraph.h"

TextGraph::TextGraph()
{
    memset (_data, ' ', GRAPH_HEIGHT*GRAPH_WIDTH);
    _column = 0;
}

/*  Convert a sample value from the range (_min - _max) to (0-GRAPH_HEIGHT) */
int TextGraph::_range (int x)
{
    x = GRAPH_HEIGHT * (x - _min) / (_max - _min);
    if (x < 0) x = 0;
    if (x >= GRAPH_HEIGHT) x = GRAPH_HEIGHT - 1;
    return x;
}

void TextGraph::add (int sample, int min, int max, int zero)
{
    if (_column == GRAPH_WIDTH)
        return;

    sample = _range(sample);
    min = _range(min);
    max = _range(max);
    zero = _range(zero);

    _data[_column][min] = 'L';
    _data[_column][max] = 'H';
    _data[_column][zero] = 'Z';
    _data[_column][sample] = '+';
    _column++;
}

void TextGraph::remove (int count)
{
    if (count > _column)
        _column = 0;
    else
        _column -= count;

    for (int y = 0; y < GRAPH_HEIGHT; y++)
        for (int x = 0; x < GRAPH_WIDTH; x++)
        {
            if (x < _column)
                _data[x][y] = _data[x+count][y];
            else
                _data[x][y] = ' ';
        }
}

/*  Draw a vertical bar at the current position.  Only over-write if spaces are
 *  present */
void TextGraph::vertical (char c)
{
    if (_column) _column--;

    for (int y = 0; y < GRAPH_HEIGHT; y++)
        if (_data[_column][y] == ' ')
            _data[_column][y] = c;

    _column++;
}

void TextGraph::draw (void)
{
    for (int x = 0; x < _column; x++)
        printf ("%1d", (x%32) / 10);

    printf ("\n");
    for (int x = 0; x < _column; x++)
        printf ("%1d", (x%32) % 10);

    printf ("\n");

    for (int y = GRAPH_HEIGHT-1; y >= 0; y--)
    {
        for (int x = 0; x < _column; x++)
            printf ("%c", _data[x][y]);

        printf ("\n");
    }
}


