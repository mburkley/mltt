#ifndef __FMDECODE_H
#define __FMDECODE_H

#include "textgraph.h"

class FMDecoder
{
public:
    static const int fifoSize = 96;
    int depth () { return _fifo.size(); }
    void add (int sample) { _fifo.push_back (sample); }
    void input (int sample, TextGraph& graph);
    void findLocalMinMax (TextGraph& graph);
    virtual void decodeBit (int bit) = 0;
    void findZeroCross (TextGraph& graph);
private:
    // bool _timingLocked;
    vector<int> _fifo;
    vector<int> _localMinMax;

};

#endif

