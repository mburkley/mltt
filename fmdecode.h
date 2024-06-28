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

/*
 *  This is the frequency modulation (FM) decoder class.  It is an astract class
 *  - users must inherit and implement a decodeBit function.
 *
 *  Samples are passed in one-by-one by calling input().  Samples are placed in
 *  a FIFO for processing.  The FIFO contains 3 frames, prev, curr and next, as
 *  signed ints.  Each frame is approx 32 samples long.  A frame
 *  can be a ONE bit or a ZERO bit.
 *
 *  The findLocalMinMax method is used to find local minima and maxima.  These
 *  are used to calculate zero cross (ZC) values which are then used to extract
 *  bit timings.
 *
 *  If a bit in a frame can be identified, the decodeBit method is called.  If
 *  not, the bit is marked as BIT_UNKOWN with the assumption that decoding the
 *  subsequent bit will give enough context to resolve the unknown bit.
 */

#ifndef __FMDECODE_H
#define __FMDECODE_H

#include "textgraph.h"

enum Bit
{
    BIT_ZERO,
    BIT_ONE,
    BIT_UNKNOWN
};

class FMDecoder
{
public:
    static const int frameSize = 32;
    static const int fifoSize = frameSize * 3;
    void input (int sample);
    virtual void decodeBit (int bit) = 0;
    void showRaw () { _showRaw = true; }
private:
    void findLocalMinMax ();
    void findZeroCross ();

    /*  Check if an offset is proximate to a position */
    bool proximity (int offset, int centre, int width);

    /*  Analyse the contents of the FIFO to identify frames */
    Bit analyseFrame (vector<int>& zc);

    /*  The samples FIFO */
    vector<int> _fifo;

    /*  A list of local minima and maxima */
    vector<int> _localMinMax;
    Bit _prevBit;

    /*  For debug, draw a graph of the wave being examined */
    TextGraph _graph;

    bool _showRaw;
};

#endif

