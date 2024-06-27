#include <iostream>
#include <vector>
#include <cmath>
using namespace std;
#include "fmdecode.h"
#include "textgraph.h"

/*  Maintain a FIFO of samples.  A frame is nominally 32 samples long and this
 *  FIFO contains 3 frames.  Previous, current and next.  */
class SampleFifo
{
private:
    vector<int> _fifo;
    vector<int> _localMinMax;

public:
    static const int size = 96;
    int depth () { return _fifo.size(); }

    void add (int sample)
    {
        #if 0
        if (_fifo.size() == size)
            _fifo.erase (_fifo.begin());
        #endif

        _fifo.push_back (sample);
    }

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

    void findLocalMinMax ();

    #if 0
    /*  Take the sample from the mid-point of the fifo.  This means we lost the
     *  first 32 samples but they are in the preamble at best so that's ok */
    int get (void)
    {
        return _fifo[size/2];
    }
    #endif

    void findZeroCross (TextGraph& graph);
};

void SampleFifo::findLocalMinMax ()
{
    int min = 32767;
    int max = -32767;
    // int lastSlope = _fifo[1] - _fifo[0];
    _localMinMax.clear ();
    bool detected = false;
    // bool isMin = false;
    enum { MIN, MAX, NONE } typ = NONE;

    for (unsigned i = 1; i < _fifo.size(); i++)
    {
        if (_fifo[i] < min)
            min = _fifo[i];
        else if (_fifo[i] > max)
            max = _fifo[i];
    }

    int mid = (max - min) / 2;

    // min += range / 4;
    // max -= range / 4;
    for (unsigned i = 1; i < _fifo.size(); i++)
    {
        #if 0
        int slope = _fifo[i] - _fifo[i-1];
        if ((slope < 0 && lastSlope > 0) || 
            (slope > 0 && lastSlope < 0))
        {
            cout << _fifo[i] << "," << _fifo[i-1] << " ";

            /*  Don't add a min/max that is less than 6 samples away from the
             *  previous as this is just noise */
            if (_localMinMax.empty() || i - _localMinMax[_localMinMax.size()-1] > 6)
                _localMinMax.push_back (i);

            lastSlope = slope;
        }
        #endif
        if (_fifo[i] < mid)
        {
            // if (!isMin)
            if (typ != MIN)
                detected = false;

            if (!detected)
            {
                _localMinMax.push_back (i);
                detected = true;
            }
            else if (_fifo[i] < _fifo[_localMinMax.back()])
                _localMinMax.back() = i;

            // isMin = true;
            typ = MIN;
        }
        if (_fifo[i] > mid)
        {
            // if (isMin)
            if (typ != MAX)
                detected = false;

            if (!detected)
            {
                _localMinMax.push_back (i);
                detected = true;
            }
            else if (_fifo[i] > _fifo[_localMinMax.back()])
                _localMinMax.back() = i;

            // isMin = false;
            typ = MAX;
        }

    }
    cout << "Min/Max ["<<min<<","<<max<<"] ";

    for (auto it : _localMinMax)
        cout << it << " ("<<_fifo[it]<<") ";

    cout << endl;
}

void SampleFifo::findZeroCross (TextGraph& graph)
{
    int state = -1;
    vector<int> zc;

    findLocalMinMax();

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
            graph.add (_fifo[i], 0, 0, 0, 0);
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

        graph.add (_fifo[i], localMin, localMax, zerocross, state);

        if (i < 17 || i > 73)
            continue;
        if (state == -1) state = _fifo[i] > zerocross;

        // cout << "i="<<i<<", smp="<<_fifo[i]<<", zc="<<zerocross<<
        //     ", amp="<<(int)amp<<", st="<<state<<endl;
        /* Apply hysteresis */
        if (state && _fifo[i] < zerocross - amp * .05)
            state = 0;
        else if (!state && _fifo[i] > zerocross + amp * .05)
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
    for (auto it : zc)
    {
        printf ("[%d (+%d)] ", it, it-offset);
        offset = it;
        if (it > 59 && it < 68)
        {
            delCount = it - 32;
            cout << "(T:" << delCount << ") ";
        }
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

    cout<<endl;

    graph.draw ();
    while (delCount--)
        _fifo.erase (_fifo.begin());
}

static SampleFifo samples;

/*  Take data from a WAV file and carve it up into bits.  Due to some recorded
 *  files having DC offsets or mains hum elements, detecting zero crossings only
 *  was found to be unreliable.  Instead, maintain local min and max vars to
 *  track sine wave peaks.  */
void FMDecode::input (int sample, TextGraph& graph)
{
    samples.add (sample);

    /*  Don't proceed until the fifo is full */
    if (samples.depth() < samples.size)
        return;

    // sample = samples.get ();

    samples.findZeroCross (graph);

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

