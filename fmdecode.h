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
    static const int fifoSize = 96;
    int depth () { return _fifo.size(); }
    void add (int sample) { _fifo.push_back (sample); }
    void input (int sample, TextGraph& graph);
    virtual void decodeBit (int bit) = 0;
private:
    // bool _timingLocked;
    void findLocalMinMax (TextGraph& graph);
    void findZeroCross (TextGraph& graph);
    bool proximity (int offset, int centre, int width);
    Bit analyseFrame (vector<int>& zc, TextGraph& graph);
    vector<int> _fifo;
    vector<int> _localMinMax;
    Bit _prevBit;
};

#endif

