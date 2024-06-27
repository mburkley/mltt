#ifndef __FMDECODE_H
#define __FMDECODE_H

#include "textgraph.h"

class FMDecode
{
public:
    void input (int sample, TextGraph& graph);
    virtual void decodeBit (int bit) = 0;
private:
    bool _timingLocked;
};

#endif

