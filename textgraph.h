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

#ifndef __TEXTGRAPH_H
#define __TEXTGRAPH_H

/*  Class to draw a simple ASCII graph of the contents of the sample fifo.
 *  Indicates high, low, zero crossing, local min/max, etc. */

#define GRAPH_WIDTH     100
#define GRAPH_HEIGHT    16

class TextGraph
{
public:
    TextGraph();
    void limits (int min, int max) { _min = min; _max = max; };
    void add (int sample, int min, int max, int zero);
    void remove (int count);
    void clear () { remove (_column); }
    void vertical (char c = '|');
    void draw (void);
private:
    int _column;
    char _data[GRAPH_WIDTH][GRAPH_HEIGHT];
    int _range (int x);
    int _min;
    int _max;
};

#endif

