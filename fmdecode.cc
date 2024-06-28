#include <iostream>
#include <vector>
#include <cmath>
using namespace std;
#include "fmdecode.h"
#include "textgraph.h"

/*  Maintain a FIFO of samples.  A frame is nominally 32 samples long and this
 *  FIFO contains 3 frames.  Previous, current and next.  */
#if 0
class SampleFifo
{
public:


    int min (void)
    {
        int min = 32767;
        // for (auto x : _fifo)
        for (int i = 0; i < 64; i++)
            if (_fifo[i] < min)
                min = _fifo[i];

        return min;
    }

    int max (void)
    {
        int max = -32768;
        // for (auto x : _fifo)
        for (int i = 0; i < 64; i++)
            if (_fifo[i] > max)
                max = _fifo[i];

        return max;
    }


    #if 0
    /*  Take the sample from the mid-point of the fifo.  This means we lost the
     *  first 32 samples but they are in the preamble at best so that's ok */
    int get (void)
    {
        return _fifo[size/2];
    }
    #endif

};
#endif

void FMDecoder::findLocalMinMax (TextGraph& graph)
{
    int min = 32767;
    int max = -32767;
    // int lastSlope = _fifo[1] - _fifo[0];
    _localMinMax.clear ();
    // bool detected = false;
    bool isMin = false;
    // enum { MIN, MAX, NONE } typ = NONE;

    for (unsigned i = 0; i < _fifo.size(); i++)
    {
        if (_fifo[i] < min)
            min = _fifo[i];
        if (_fifo[i] > max)
            max = _fifo[i];
    }

    graph.limits (min, max);
    int range = (max - min);

    // min += range / 4;
    // max -= range / 4;

    /*  To start, create a local using the first sample.  Guess whether it is a
     *  min or a max by whether it is above or below the mid range */
    _localMinMax.push_back (0);
    int peak = _fifo[0];
    isMin = (peak < min + range / 2);

    for (unsigned i = 1; i < _fifo.size(); i++)
    {
        // int peak = _fifo[_localMinMax.back()];

        /*  If this sample has a higher value then the peak then store it
         *  as a new local min / max.  */
        if ((isMin && _fifo[i] < peak) ||
            (!isMin && _fifo[i] > peak))
        {
            // if (!_localMinMax.empty())
                _localMinMax.back() = i;

            peak = _fifo[i];
        }

        /*  If this value is more than 10% less than the peak value, then
         *  create a new local min/max and change from tracking min to max or
         *  vice versa */
        if ((isMin && _fifo[i] > peak + range / 10) ||
            (!isMin && _fifo[i] < peak - range / 10))
        {
            cout << "change at "<<i<<" to "<<(isMin?"MAX":"MIN")<<endl;
            _localMinMax.push_back (i);
            peak = _fifo[i];
            isMin = !isMin;
        }
    }
    cout << "Min/Max/Rng ["<<min<<","<<max<<","<<range<<"] ";

    for (auto it : _localMinMax)
        cout << it << " ("<<_fifo[it]<<") ";

    cout << endl;
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
    // double localMin = min();
    // double localMax = max();

    // double amp = localMax - localMin;
    // double zerocross = (localMin+localMax) / 2;

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

        double amp = abs (localMax - localMin);
        double zerocross = (localMin+localMax) / 2;

        graph.add (_fifo[i], localMin, localMax, zerocross);

        if (i < 17 || i > 73)
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

    /*  We should see at least 2 zero crossings in the fifo */
    #if 0
    if (zc.size () < 2) //  || zc[0] > 48)
    {
        cout << "DISCARD - no zc"<<endl;
        _fifo.clear ();
        return;
    }
    #endif
    cout << "ZC : ";
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
        if (*it >= 60 && *it < 68)
        {
            delCount = *it - 32;
            cout << "(T:" << delCount << ") ";
        }

        /*  Is there a ZC in the middle of the bit?  If so, this is a ONE */
        if (*it >= 40 && *it < 56)
            isOne = true;

        if (*it >= 24 && *it < 40)
            start = it - zc.begin();

        if (*it >= 56 && *it < 72)
            end = it - zc.begin();
    }

    bool success = false;
    int bit;
    if (zc.size() == 2 && (end - start) == 1 && !isOne)
    {
        bit = 0;
        success = true;
    }

    if (zc.size() == 3 && (end - start) == 2 && isOne)
    {
        bit = 1;
        success = true;
    }

    if (success)
    {
        cout<<endl;

        decodeBit (bit);
    }
    else
    {
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
    }

    while (delCount--)
        _fifo.erase (_fifo.begin());
}

// static SampleFifo samples;

/*  Take data from a WAV file and carve it up into bits.  Due to some recorded
 *  files having DC offsets or mains hum elements, detecting zero crossings only
 *  was found to be unreliable.  Instead, maintain local min and max vars to
 *  track sine wave peaks.  */
void FMDecoder::input (int sample, TextGraph& graph)
{
    add (sample);

    /*  Don't proceed until the fifo is full */
    // if (samples.depth() < samples.size)
    if (depth() < fifoSize)
        return;

    // sample = samples.get ();

    findZeroCross (graph);

        #if 0
        _graph.add (sample, localMin, localMax, zerocross, state);

        if (state == lastState)
        {
            changeCount++;
            continue;
        }

        /*  Found a frame, analyse it */
        _graph.vertical();
        inputBitWidth (changeCount, i);
        lastState = state;
        changeCount = 0;
    }
    #endif

}

