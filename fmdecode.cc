#include <iostream>
#include <vector>
#include <cmath>
using namespace std;
#include "fmdecode.h"
#include "textgraph.h"

bool FMDecoder::proximity (int offset, int centre, int width)
{
    if (offset >= centre - width && offset < centre + width)
        return true;

    return false;
}

void FMDecoder::findLocalMinMax (TextGraph& graph)
{
    int min = 32767;
    int max = -32767;
    _localMinMax.clear ();
    bool isMin = false;

    for (unsigned i = 0; i < _fifo.size(); i++)
    {
        if (_fifo[i] < min)
            min = _fifo[i];
        if (_fifo[i] > max)
            max = _fifo[i];
    }

    graph.limits (min, max);
    int range = (max - min);

    /*  To start, create a local using the first sample.  Guess whether it is a
     *  min or a max by whether it is above or below the mid range */
    _localMinMax.push_back (0);
    int peak = _fifo[0];
    isMin = (peak < min + range / 2);

    for (unsigned i = 1; i < _fifo.size(); i++)
    {
        /*  If this sample has a higher value then the peak then store it
         *  as a new local min / max.  */
        if ((isMin && _fifo[i] < peak) ||
            (!isMin && _fifo[i] > peak))
        {
            _localMinMax.back() = i;
            peak = _fifo[i];
        }

        /*  If this value is more than 10% less than the peak value, then
         *  create a new local min/max and change from tracking min to max or
         *  vice versa */
        if ((isMin && _fifo[i] > peak + range / 10) ||
            (!isMin && _fifo[i] < peak - range / 10))
        {
            _localMinMax.push_back (i);
            peak = _fifo[i];
            isMin = !isMin;
        }
    }
}

Bit FMDecoder::analyseFrame (vector<int>& zc, TextGraph& graph)
{
    Bit bit = BIT_UNKNOWN;

    /*  We should see at least 2 zero crossings in the fifo */
    #if 1
    if (zc.size () < 2) //  || zc[0] > 48)
    {
        cout << "DISCARD - no zc"<<endl;
        _fifo.clear ();
        return BIT_UNKNOWN;
    }
    #endif

    cout << "ZC : ";

    /*  If the last bit was ambiguous */
    if (_prevBit == BIT_UNKNOWN && 
       ((zc.size () == 2 && proximity (zc[1], 64, 8)) ||
        (zc.size () == 3 && proximity (zc[2] - zc[1], 16, 8))))
    {
        _prevBit = BIT_ZERO;
        cout << "PREV ZERO ";
        decodeBit (0);
    }

    /*  If the first ZC is due to previous frame being a ONE then drop the first
     *  ZC.  Also if the 2nd ZC is exactly at the start of a frame */
    if (zc.size() >= 2 &&
       ((_prevBit == BIT_ONE && zc[0] < 24) ||
        proximity (zc[1], 32, 8)))
    {
        cout << "drop zc0 "<<zc[0] << " ";
        zc.erase (zc.begin());
    }

    /*  Drop any ZCs after the 3rd one */
    #if 1
    // int last = zc.size() - 1;
    while (zc.size() > 3) //  && zc[last-1] >= 56 && zc[last-1] > 72)
    {
        cout << "drop end zc ";
        zc.erase (zc.begin()+3);
    }
    #endif

    /*  Drop the 3rd ZC if we are happy the 2nd aligns with an end of frame */
    if (zc.size() == 3 && proximity (zc[1], 64, 8))
    {
        cout << "drop superf end zc ";
        zc.erase (zc.begin()+2);
    }

    int delCount = 32;
    int offset = 0;
    bool isOne = false;
    int start = 0, end = 0;
    // for (auto it : zc)
    for (auto it = zc.begin(); it != zc.end(); ++it)
    {
        printf ("[%d (+%d)] ", *it, *it-offset);
        offset = *it;

        /*  Timing recovery.  If a ZC is close to where we expect it to be for
         *  two frames, then use it as a timing ref.  Two frames = 64 samples so use it
         *  if between 60 and 68 */
        if (proximity (*it, 64, 4))
        {
            delCount = *it - 32;
            cout << "(T:" << delCount << ") ";
        }

        /*  Is there a ZC in the middle of the bit?  If so, this is a ONE */
        // if (*it >= 40 && *it < 56)
        //     isOne = true;

        if (proximity (*it, 32, 8))
            start = it - zc.begin();

        if (proximity (*it, 64, 8))
            end = it - zc.begin();
    }

    while (delCount--)
        _fifo.erase (_fifo.begin());

    // bool success = false;
    if (zc.size() == 2 && end != 1 && proximity (zc[1], 64, 16))
    {
        /*  If the last ZC is early or late, then we may not think it is an end of
         *  frame.  But if it is the last ZC in our list then the following
         *  frame has been stretched or compressed, so assume this is a valid end of
         *  frame */
        end = 1;
        cout << "force end"<<endl;
        graph.draw ();
    }

    if (zc.size() == 2 && (end - start) == 1) // && !isOne)
    {
        // success = true;
        cout << "VALID ZERO" << endl;
        _prevBit = BIT_ZERO;
        return BIT_ZERO;
    }

    else if (zc.size() == 3 && (end - start) == 2) // && isOne)
    {
        // success = true;
        cout << "VALID ONE" << endl;
        _prevBit = BIT_ONE;
        return BIT_ONE;
    }

    // if (delCount == 0)
    //     delCount = 32;
    // delCount = zc[0];
    if (zc.size() == 2)
        cout<< "ZERO";
    else if (zc.size() == 3)
        cout<< "ONE";
    else
        cout << "UNKNOWN";

    cout << (isOne ? " ZC:ONE" : " ZC:ZERO");

    cout << " D:"<<end-start;
    if (end - start == 1)
        cout << " ST:ZERO";
    else if (end - start == 2)
        cout << " ST:ONE";
    else
        cout << " ST:UNKNOWN";
    cout<<endl;
    graph.draw ();

    _prevBit = bit;
    return bit;
}

void FMDecoder::findZeroCross (TextGraph& graph)
{
    int state = -1;
    vector<int> zc;

    findLocalMinMax(graph);

    /*  If we do not have a local min and max in the fifo, then dump the fifo
     *  and return */
    if (_localMinMax.size() < 2)
    {
        cout << "DISCARD - no min/max"<<endl;
        _fifo.clear ();
        return;
    }

    int index = 0;

    graph.clear();
    // graph.vertical ();

    // state = _fifo[0] > zerocross;

    for (int i = 0; i < (int) _fifo.size(); i++)
    // for (unsigned i = 24; i < 72; i++)
    {
        /*  If a sample lies outside known local min or max then we can't
         *  process it so continue */
        if (i <= _localMinMax.front() || i >= _localMinMax.back())
        {
            graph.add (_fifo[i], 0, 0, 0);
            continue;
        }

        /*  If the sample lies beyond the current local min/max then advance the
         *  index.  It can't be beyond the last one so it is safe to increment
         *  the index */
        if (i > _localMinMax[index+1])
        {
            index++;
            graph.vertical('m');
        }

        int localMin = _fifo[_localMinMax[index]];
        int localMax = _fifo[_localMinMax[index+1]];

        // double amp = abs (localMax - localMin);
        double zerocross = (localMin+localMax) / 2;

        graph.add (_fifo[i], localMin, localMax, zerocross);

        if (i < 8 || i > 88)
            continue;

        if (state == -1) state = _fifo[i] > zerocross;

        // cout << "i="<<i<<", smp="<<_fifo[i]<<", zc="<<zerocross<<
        //     ", amp="<<(int)amp<<", st="<<state<<endl;
        /* Apply hysteresis */
        if (state && _fifo[i] < zerocross) // - amp * .05)
            state = 0;
        else if (!state && _fifo[i] > zerocross) // + amp * .05)
            state = 1;
        else
            continue;

        graph.vertical();
        zc.push_back (i);
    }

    Bit bit = analyseFrame (zc, graph);

    if (bit != BIT_UNKNOWN)
        decodeBit (bit);

}

/*  Take data from a WAV file and carve it up into bits.  Due to some recorded
 *  files having DC offsets or mains hum elements, detecting zero crossings only
 *  was found to be unreliable.  Instead, maintain local min and max vars to
 *  track sine wave peaks.  */
void FMDecoder::input (int sample, TextGraph& graph)
{
    add (sample);

    /*  Don't proceed until the fifo is full */
    if (depth() < fifoSize)
        return;

    findZeroCross (graph);
}

