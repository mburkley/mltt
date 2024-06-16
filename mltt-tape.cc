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
 *  Tool to read or create cassette audio files.  Samples are read from a WAV
 *  file and broken up into frames.  Each frame is a half a sine wave.  A frame
 *  can be half a ONE bit or a full ZERO bit.  A frame is typically 16 or 32
 *  samples long.
 *
 *  Samples are read into a FIFO with 64 enties.  This FIFO is used to find the
 *  highest and lowest values in a frame.  The zero-crossing point is the
 *  midpoint of the high and the low.  Once a zero crossing is found, the frame
 *  is added to another FIFO for processing.
 *
 *  The frame FIFO keeps a number of processed and unprocessed frames.  This is
 *  so that the first unprocessed frame can be analysed within the context of
 *  previous and next frames.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <iostream>
#include <vector>

using namespace std;

#include "types.h"
#include "wav.h"
#include "cassette.h"
#include "parse.h"
#include "files.h"
#include "tibasic.h"
#include "disk.h"
#include "textgraph.h"

class TapeDecode
{
private:
    bool _verbose;
    bool _showRaw;

    bool _errorsFound;
    bool _errorsUnfixable;
    bool _preambleSync;
    int _preambleBitsExpected;

    bool _haveHeader;

    int _headerBytes;
    int _blockBytes;
    int _bitCount;
    int _blockCount;
    int _blockIndex;
    int _recordCount;
    int _programSize;

    struct
    {
        uint8_t mark;
        uint8_t size1;
        uint8_t size2;
    }
    _header;

    struct __block
    {
        // uint8_t sync[8];
        uint8_t mark;
        uint8_t data[64];
        uint8_t checksum;
    }
    _block[2];

    Files *_file;
    TextGraph _graph;

    void decodeBlock (int byte);
    void decodeBit (int bit);
    void inputBitWidth (int count, int position);
    void outputByte (uint8_t byte);
    void outputBlock (uint8_t *data);
public:
    TapeDecode ()
    {
    }
    void setVerbose () { _verbose = true; }
    void setShowRaw () { _showRaw = true; }
    void setPreamble () { _preambleSync = true; }
    void setPreambleBits (int bits) { _preambleBitsExpected = bits; }
    void outputWav (uint8_t *inputData, int inputSize);
    bool inputWav (Files *outputFile, const char *inputName, bool showParams);
    void showResult ();
};

static Cassette cassette;
static TapeDecode tape;

void TapeDecode::decodeBlock (int byte)
{
    if (!_haveHeader)
    {
        ((uint8_t *)&_header)[_headerBytes++] = byte;
        if (_headerBytes == 3)
        {
            if (_verbose) cout << "Reading " << _header.size1 << " blocks ..." << endl;
            _haveHeader = true;
            _preambleSync = true;
            _preambleBitsExpected = 40;
        }
        return;
    }

    ((uint8_t *)&_block[_blockIndex])[_blockBytes++] = byte;
    if (_blockBytes == sizeof (struct __block))
    {
        _blockIndex++;
        _blockBytes = 0;

        /*  Turn back on preamble synchronisatoin to sync with the next data
         *  block */
        _preambleSync = true;
        _preambleBitsExpected = 40;
    }


    /*  Do we have two complete blocks?  If so, compare them and calcualte
     *  checksums */
    if (_blockIndex == 2)
    {
        if (_verbose)
            cout << "\nBlock " << _blockCount << endl;

        int sum1 = 0;
        int sum2 = 0;

        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                uint8_t byte1 = _block[0].data[i*8+j];
                uint8_t byte2 = _block[1].data[i*8+j];

                if (_verbose)
                {
                    /*  If bytes in both copies are not identical, print both */
                    if (byte1 != byte2)
                        printf ("[%2X != %02X] ", byte1, byte2);
                    else
                        printf ("%02X ", byte1);
                }

                sum1 += byte1;
                sum2 += byte2;
            }

            if (_verbose)
            {
                for (int j = 0; j < 8; j++)
                {
                    uint8_t byte = _block[0].data[i*8+j];

                    printf ("%c", (byte >= 32 && byte < 128) ? byte : '.');
                }

                cout << endl;
            }
        }

        if (_block[0].mark != 0xFF) // || block[0].sync[0] != 0)
        {
            cerr << "*** Block " << _blockCount << " is misaligned" << endl;
            _errorsFound = true;
            _errorsUnfixable = false;
        }

        if (memcmp (&_block[0], &_block[1], sizeof (struct __block)))
        {
            cerr << "*** Block " << _blockCount << " copies don't match" << endl;
            _errorsFound = true;
        }


        sum1 &= 0xff;
        sum2 &= 0xff;

        if (sum1 != _block[0].checksum)
            printf ("BAD checksum=%02X copy 1, expected %02X\n", _block[0].checksum, sum1);

        if (sum2 != _block[1].checksum)
            printf ("BAD checksum=%02X copy 2, expected %02X\n", _block[1].checksum, sum2);

        sum1 -= _block[0].checksum;
        sum2 -= _block[1].checksum;

        /*  At this point both sum1 and sum2 should be zero.  If either or
         *  both is non zero, print an error.  If only one is bad, copy from
         *  the good to the bad.  */
        if (!sum1 && !sum2)
        {
            if (_verbose)
                printf ("GOOD checksum=%02X\n", _block[0].checksum);
        }
        else if (sum1 && sum2)
        {
            cerr << "*** Block " << _blockCount << ", both copies have BAD checksum" << endl;
            _errorsFound = true;
            _errorsUnfixable = false;
        }
        else if (sum1)
        {
            cerr << "*** Block " <<  _blockCount << ", first copy has BAD checksum, using second copy" << endl;
            memcpy (_block[0].data, _block[1].data, 64);
            _errorsFound = true;
        }
        else if (sum2)
        {
            cerr << "*** Block " << _blockCount << ", second copy has BAD checksum, ignoring" << endl;
            _errorsFound = true;
        }

        memcpy (_file->getData () + _blockCount * 64, _block[0].data, 64);
        _blockIndex = 0;
        _blockCount++;
    }
}

void TapeDecode::decodeBit (int bit)
{
    static int byte;
    static int preambleCount;

    if (bit && _preambleSync)
    {
        if (_showRaw) cout << "preamble count=" << preambleCount << " of " << _preambleBitsExpected << endl;
        /*  We expect the preamble to be 6144 bits at the start of the file and
         *  64 bits at the start of each block.  So if longer than 3072 bits we
         *  assume new record.  */
        if (preambleCount > 3072)
        {
            _recordCount++;

            if (_haveHeader)
            {
                _haveHeader = false;
                _headerBytes = 0;
            }
        }

        /*  Reset the preamble bit count.  If we have seen at least 60 zero bits in a row we consider this to
         *  be a valid preamble and start reading data.  */
        if (preambleCount > _preambleBitsExpected)
            _preambleSync = false;

        preambleCount = 0;
    }

    if (_preambleSync)
    {
        preambleCount++;
        return;
    }

    byte <<= 1;
    byte |= bit;
    _bitCount++;

    if (_bitCount == 8)
    {
        if (_showRaw)
            printf ("BYTE %02X (%d/%ld)\n", byte, _blockBytes, sizeof (struct __block));
        decodeBlock (byte);
        byte = 0;
        _bitCount = 0;
    }
}

class SampleFifo
{
private:
    vector<int> _fifo;

public:
    static const int size = 64;
    void add (int sample)
    {
        if (_fifo.size() == size)
            _fifo.erase (_fifo.begin());

        _fifo.push_back (sample);
    }

    int min (void)
    {
        int min = 32767;
        for (auto x : _fifo)
            if (x < min)
                min = x;

        return min;
    }

    int max (void)
    {
        int max = -32768;
        for (auto x : _fifo)
            if (x > max)
                max = x;

        return max;
    }

    /*  Take the sample from the mid-point of the fifo.  This means we lost the
     *  first 32 samples but they are in the preamble at best so that's ok */
    int get (void)
    {
        return _fifo[size/2];
    }
};

static SampleFifo samples;

class FrameFifo
{
private:
    struct _frame
    {
        int width;
        int error;
        bool done;
        char sym;
    } ;
    vector<struct _frame> _fifo;
public:
    const int maxSize = 5;

    void remove (int n)
    {
        for (int i = 0; i < n; i++)
            if (_fifo.size() > 0)
                _fifo.erase(_fifo.begin());
    }

    void add (int width, char sym)
    {
        struct _frame f;
        f.width = width;
        f.error = 30 - width;
        f.done = false;
        f.sym = sym;
        _fifo.push_back (f);
    }

    int size ()
    {
        return (int) _fifo.size();
    }

    void show ()
    {
        printf ("\"");
        for (auto it : _fifo)
            printf ("%d=%c ", it.width, it.sym);
        printf ("\"");
    }

    int findNext (TextGraph& graph)
    {
       /*  Find first unprocessed frame */
        int next = 0;
        while (_fifo[next].done)
            next++;
        
        /*  Keep no more than 2 finished frames */
        while (next > 2)
        {
            graph.remove (_fifo[0].width + 1);
            remove (1);
            next--;
        }

        return next;
    }

    void identify (int index, char sym)
    {
        _fifo[index].done = true;
        _fifo[index].sym = sym;

        /*  If symbol is first or second half of a ONE frame then we have
         *  overestimated the error so reduce by half a frame */
        if (sym == 'A' || sym == 'B')
            _fifo[index].error -= 15;
    }

    int sumForward (int from, int count)
    {
        int sum = 0;

        for (int i = 0; i < count; i++)
            sum += _fifo[from+i].width;

        return sum;
    }

    int width (int index)
    {
        return _fifo[index].width;
    }
    int error (int index)
    {
        return _fifo[index].error;
    }
};

static FrameFifo frames;

/*  Analyse the width of a frame to determine if it is a ZERO or a ONE.  ONEs
 *  must occur in pairs.  If it is unclear whether the frame being analysed is
 *  clearly a ONE or a ZERO then look at the errors in previous and next frames
 *  to try and make it more clear. */
void TapeDecode::inputBitWidth (int width, int position)
{
    // static int bitCount = 0;
    bool haveBit = false;
    int bit = 0;

    frames.add (width, '?');
    #if 0
    struct _frame f;
    f.width = width;
    f.error = 30 - width;
    f.done = false;
    f.sym = '?';
    frameFifo.push_back (f);
    #endif

    #if 0
    int errsum = 0;
    for (int i = 0; i < 4; i++)
    {
        errsum += lastError[i];
        lastError[i] = lastError[i+1];
    }
    lastError[3] = 30 - width;
    #endif

    // if (_showRaw) 
    // printf ("[%d e:%d]\n", width, errsum);
    // printf ("[%d,q:%ld]\n", width, frameFifo.size());
    // _graph.vertical();

    /*  Maintain a fifo of frames so we can look at frames before and after the
     *  one we are analysing */
    if (frames.size () < 7) // SYMBOL_FIFO)
    {
        // if (_showRaw)
        // printf ("queued\n");
        return;
    }

    if (_showRaw)
    {
        _graph.draw ();
        // _graph.vertical ();

        frames.show ();
    }

    int next = frames.findNext (_graph);
    // printf ("examine frames %d and %d of %ld\n", next, next+1, frameFifo.size());
    int sum = frames.sumForward (next, 2);
    int sumNext = frames.sumForward (next+1, 2);

    if (sum < 40)
    {
        /*  The sum of this frame and the next frame is short enough that both
         *  are ONE so identify both */
        if (_showRaw)
            printf ("[%d+%d=%d ONE]\n", frames.width (next), frames.width (next+1), sum);

        bit = 1;
        haveBit = true;
        frames.identify (next, 'A');
        frames.identify (next+1, 'B');
        // frameFifo[next].done = true;
        // frameFifo[next].sym = 'A';
        // frameFifo[next].error -= 15;
        // frameFifo[next+1].done = true;
        // frameFifo[next+1].sym = 'B';
        // frameFifo[next+1].error -= 15;
        // _graph.remove (frameFifo[1].width);
        // frameFifoRemove (2);
    }
    else if (sum > 56)
    {
        /*  The sum of this frame and the next frame is long enough that both
         *  must be ZERO so identify this frame as ZERO */
        if (_showRaw)
            printf ("[%d+%d=%d ZERO]\n", frames.width (next), frames.width (next+1), sum);

        bit = 0;
        haveBit = true;
        frames.identify (next, '0');
        // frameFifo[next].done = true;
        // frameFifo[next].sym = '0';
        // _graph.remove (frameFifo[1].width);
        // framFifoRemove (1);
    }
    else if (frames.width (next) > 23)
    {
        /*  This frame is long enough that it must be a ZERO so identify it */
        if (_showRaw)
            printf ("[%d ZERO]\n", frames.width (next));

        bit = 0;
        haveBit = true;
        frames.identify (next, '0');
        // frameFifo[next].done = true;
        // frameFifo[next].sym = '0';
        // _graph.remove (frameFifo[1].width);
        // fifoWidthRemove (1);
    }
    else
    {
        /*  This frame seems to be too short to be a ONE, look at frames before
         *  and after this one */
        if (frames.width (next+1) > 30)
        {
            if (_showRaw)
                printf ("[%d short ZERO]\n", frames.width (next));
            /* Next frame is longer than a ZERO, so we can't be a ONE, we must be a
             * shortened ZERO */
            bit = 0;
            haveBit = true;
            // frameFifo[next].done = true;
            // frameFifo[next].sym = '0';
            frames.identify (next, '0');
        }
        else
        {
            /*  This frame is too short to be ZERO but the next frame may not be a
             *  ONE.  Take the error from the previous frame into account and see
             *  does that make this frame more likely to be to a ZERO */
            int err = frames.error (next-1);
            if (sumNext < 45)
            {
                /*  The next two frames seem to be ONEs.  Add their error and see
                 *  does that clarify things */
                err += 15 - sumNext;

                if (frames.width (next) - err < 22)
                {
                    /*  Taking the error from previous and next frames into
                     *  account indicates we are short enough to be a ONE which
                     *  means the next frame must also be a ONE */
                    if (_showRaw)
                        printf ("[%d,%d context ONE]\n", frames.width (next),
                        frames.width(next+1));
                    bit = 1;
                    haveBit = true;
                    frames.identify (next, 'A');
                    frames.identify (next+1, 'B');
                    // frameFifo[next].done = true;
                    // frameFifo[next].sym = '0';
                    // frameFifo[next].done = true;
                    // frameFifo[next].sym = 'A';
                    // frameFifo[next].error -= 15;
                    // frameFifo[next+1].done = true;
                    // frameFifo[next+1].sym = 'B';
                    // frameFifo[next+1].error -= 15;
                }
                else if (frames.width (next) - err > 27)
                {
                    /*  Taking the error from previous and next makes it look
                     *  like we are a ZERO so identify it */
                    if (_showRaw)
                        printf ("[%d context ZERO]\n", frames.width (next));
                    bit = 0;
                    haveBit = true;
                    frames.identify (next, '0');
                    // frameFifo[next].done = true;
                    // frameFifo[next].sym = '0';
                }
            }
        }
    }

    if (!haveBit)
    {
        /*  We still don't know what this frame is.  Identify it as X and bale out */
        if (_showRaw) 
            printf (" : [%d=???] pos=%d prev-err=%d nextw=%d new-w=%d\n",
                    frames.width (next), position, frames.error (next-1), frames.width
                    (next+1), sumNext);

        frames.identify (next, 'X');
        // frameFifo[next].done = true;
        // frameFifo[next].sym = 'X';
        // _graph.remove (frameFifo[1].width);
        // fifoWidthRemove (1);
        return;
    }

    if (_showRaw) printf ("BIT %d = %d\n", _blockBytes*8+_bitCount, bit);
    decodeBit (bit);
    if (_showRaw) printf ("=======================\n");
}

/*  Take data from a WAV file and carve it up into bits.  Due to some recorded
 *  files having DC offsets or mains hum elements, detecting zero crossings only
 *  was found to be unreliable.  Instead, maintain local min and max vars to
 *  track sine wave peaks.  Apply hysteresis to the detection so switch the bit
 *  from a zero to a one or vice versa once 50% of the peak is seen.  To apply
 *  gain control, the peak is diminished by 1% per sample, which is about 72%
 *  per bit (32 samples).  We look for 80% of peak which is a relative
 *  ampltitude of 0.5 before changing state. */
bool TapeDecode::inputWav (Files *outputFile, const char *inputName, bool showParams)
{
    WavFile wav;
    int16_t sample;
    int changeCount = 0;
    int state = 0;
    // int slope = 0; // 0 = upward, 1 = downward
    int lastState = 0;
    // double localMin = 0;
    // double localMax = 0;

    _file = outputFile;

    if (!wav.openRead (inputName, showParams))
    {
        cerr << "read fail" << endl;
        return false;
    }

    for (int i = 0; i < samples.size; i++)
    {
        sample = wav.readSample ();
        samples.add (sample);
    }

    for (int i = 0; i < wav.getSampleCount (); i++)
    {
        // localMax = 0.99 * localMax;  // 0.99 ^ 32 = 0.725
        // localMin = 0.99 * localMin;

        samples.add (wav.readSample ());
        sample = samples.get ();

        double localMin = samples.min();
        double localMax = samples.max();

        double amp = localMax - localMin;
        double zerocross = (localMin+localMax) / 2;
        // Apply hysteresis
        if (state && sample < zerocross - amp * .05)
            state = 0;

        if (!state && sample > zerocross + amp * .05)
            state = 1;

        _graph.add (sample, localMin, localMax, zerocross, state);

        if (state == lastState)
        {
            changeCount++;
            continue;
        }

        // if (_showRaw) printf ("(%d,%d,%d)\n", (int) localMin, (int) zerocross, (int) localMax);

        // Found a potential bit, analyse it
        _graph.vertical();
        inputBitWidth (changeCount, i);
        lastState = state;
        changeCount = 0;
    }

    /*  Do a final call to process the width of the last sample */
    inputBitWidth (changeCount, 0);
    wav.close ();

    if (!_file->setSize (_blockCount * 64))
    {
        cerr << "resize fail" << endl;
        return false;
    }

    return true;
}

#define MAX_PROGRAM_SIZE    0x4000
#define BIT_DURATION        730000 // 730 usec = 1370 Hz

void TapeDecode::outputByte (uint8_t byte)
{
    /*  Output bits from MSB thru LSB */
    for (int i = 7; i >= 0; i--)
    {
        cassette.modulationToggle();
        cassette.timerExpired (BIT_DURATION/2);

        if (byte & (1<<i))
            cassette.modulationToggle();

        cassette.timerExpired (BIT_DURATION/2);
    }
}

void TapeDecode::outputBlock (uint8_t *data)
{
    int i;

    for (i = 0; i < 8; i++)
        outputByte (0);

    outputByte (0xFF);

    uint8_t sum = 0;

    for (i = 0; i < 64; i++)
    {
        outputByte (data[i]);
        sum += data[i];
    }

    outputByte (sum);
}

void TapeDecode::outputWav (uint8_t *inputData, int inputSize)
{
    int i;

    /*  Preamble */
    for (i = 0; i < 768; i++)
        outputByte (0);

    _recordCount = 1;
    _blockCount = (inputSize + 1) / 64;

    /*  Output header */
    outputByte (0xFF);
    outputByte (_blockCount);
    outputByte (_blockCount);

    for (i = 0; i < _blockCount; i++)
    {
        outputBlock (inputData + i * 64);
        outputBlock (inputData + i * 64);
    }
}

void TapeDecode::showResult ()
{

    cerr << "Found " << _blockCount << " blocks in " << _recordCount << " record" << 
            (_recordCount > 1 ? "s":"") <<
            (_errorsFound ? "" : ", no errors were found") << endl;

    if (_errorsFound)
        cerr << "*** Errors were found which " <<
                 (_errorsUnfixable ? "can't be fixed" : "are fixable") << endl;
}

int main (int argc, char *argv[])
{
    char c;
    const char *binFileName = "";
    bool create = false;
    bool extract = false;
    bool showWav = false;
    bool verbose = false;
    bool showRaw = false;
    bool tifiles = false;

    while ((c = getopt(argc, argv, "c:e:vrwtz")) != -1)
    {
        switch (c)
        {
            case 'c' : create = true; binFileName = optarg; break;
            case 'e' : extract = true; binFileName = optarg; break;
            case 'v' : verbose = true; break;
            case 'r' : showRaw = true; break;
            case 'w' : showWav = true; break;
            case 't' : tifiles = true; break;
            default: cerr << "Unknown option '" << c << "'" << endl;
        }
    }

    if (argc - optind < 1)
    {
        cout << "\nTool to read and write wav files for cassette audio\n\n" 
                "usage: " << argv[0] << " [-c <file>] [-e <file>] [-vrwt] <wav-file>\n" 
                "\t where -c=create WAV from <file> (TIFILES or tokenised TI basic)\n"
                "\t       -e=extract to <file>, -v=verbose, -t=add tifiles header,\n"
                "\t       -r=raw bits, -w=show wav hdr\n\n" << endl;
        return 1;
    }

    if (verbose)
        tape.setVerbose ();

    if (showRaw)
        tape.setShowRaw ();

    if (create)
    {
        struct stat statbuf;
        if (stat (argv[optind], &statbuf) != -1)
        {
            cerr << "File " << argv[optind] << " exists, won't over-write" << endl;
            return 1;
        }

        Files file (binFileName, true, verbose);
        file.read (); // sReadBinary (binFileName, &program, NULL, options.verbose);

        if (file.getSize () < 0)
            return 1;

        cassette.fileOpenWrite (argv[optind]);
        tape.outputWav (file.getData (), file.getSize ());
        cassette.fileCloseWrite ();
    }
    else
    {
        Files file (binFileName, tifiles, verbose);
        file.realloc (0x4000); // Max program size 16k
        tape.setPreamble ();
        tape.setPreambleBits (3000);

        if (!tape.inputWav (&file, argv[optind], showWav))
        {
            cerr << "Can't read input file " << argv[optind] << endl;
            return 1;
        }

        file.initTifiles (binFileName, file.getSize(), DISK_BYTES_PER_SECTOR, 0, true, false);
        if (extract)
        {
            if (file.write () < 0)
            {
                cerr << "Failed to write output file " << binFileName << endl;
                return 1;
            }

            if (!tifiles)
                file.setxattr ();
        }

        if (verbose)
            printf ("\n");

        tape.showResult ();
    }

    return 0;
}
