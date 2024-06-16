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
 *  Tool to read or create cassette audio files.
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

Cassette cassette;

class Tape
{
private:
    bool _verbose;
    bool _showRaw;

    bool _errorsFound = false;
    bool _errorsFixable = true;
    bool _preambleSync = false;
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

    void decodeBlock (int byte);
    void decodeBit (int bit);
    void inputBitWidth (int count);
    void outputByte (uint8_t byte);
    void outputBlock (uint8_t *data);
public:
    Tape ()
    {
        memset (this, 0, sizeof (*this));
        _errorsFixable = true;
    }
    void setVerbose () { _verbose = true; }
    void setShowRaw () { _showRaw = true; }
    void setPreamble () { _preambleSync = true; }
    void setPreambleBits (int bits) { _preambleBitsExpected = bits; }
    void outputWav (uint8_t *inputData, int inputSize);
    bool inputWav (Files *outputFile, const char *inputName, bool showParams);
    void showResult ();
};

void Tape::decodeBlock (int byte)
{
    if (!_haveHeader)
    {
        ((uint8_t *)&_header)[_headerBytes++] = byte;
        if (_headerBytes == 3)
        {
            if (_verbose) printf ("Reading %d blocks ...", _header.size1);
            _haveHeader = true;
            _preambleSync = true;
            _preambleBitsExpected = 40;
        }
        return;
    }

    ((uint8_t *)&_block[_blockIndex])[_blockBytes++] = byte;
    // printf("recbytes=%d ", blockBytes);
    if (_blockBytes == sizeof (struct __block))
    {
        // printf("adv rec\n");
        _blockIndex++;
        _blockBytes = 0;

        /*  Turn back on preamble synchronisatoin to sync with the next data
         *  block
         */
        _preambleSync = true;
        _preambleBitsExpected = 40;
    }


    /*  Do we have two complete blocks?  If so, compare them and calcualte
     *  checksums
     */
    if (_blockIndex == 2)
    {
        // printf ("decoding\n");
        if (_block[0].mark != 0xFF) // || block[0].sync[0] != 0)
        {
            fprintf (stderr, "*** Block %d misaligned\n", _blockCount);
            _errorsFound = true;
            _errorsFixable = false;
        }

        if (memcmp (&_block[0], &_block[1], sizeof (struct __block)))
        {
            fprintf (stderr, "*** Block %d mismatch\n", _blockCount);
            _errorsFound = true;
        }

        if (_verbose)
            printf ("\nBlock %d:\n", _blockCount);

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

                printf ("\n");
            }
        }

        sum1 &= 0xff;
        sum2 &= 0xff;
        sum1 -= _block[0].checksum;
        sum2 -= _block[1].checksum;

        /*  At this point both sum1 and sum2 should be zero.  If either or
         *  both is non zero, print an error.  If only one is bad, copy from
         *  the good to the bad.
         */
        if (!sum1 && !sum2)
        {
            if (_verbose)
                printf ("GOOD checksum=%02X\n", _block[0].checksum);
        }
        else if (sum1 && sum2)
        {
            fprintf (stderr, "*** Block %d, both copies have BAD checksum\n", _blockCount);
            _errorsFound = true;
            _errorsFixable = false;
        }
        else if (sum1)
        {
            fprintf (stderr, "*** Block %d, first copy has BAD checksum, using second copy\n", _blockCount);
            memcpy (_block[0].data, _block[1].data, 64);
            _errorsFound = true;
        }
        else if (sum2)
        {
            fprintf (stderr, "*** Block %d, second copy has BAD checksum, ignoring\n", _blockCount);
            _errorsFound = true;
        }

        memcpy (_file->getData () + _blockCount * 64, _block[0].data, 64);
        _blockIndex = 0;
        _blockCount++;
    }
}

void Tape::decodeBit (int bit)
{
    static int byte;
    static int preambleCount;

    if (bit && _preambleSync)
    {
        if (_showRaw) printf ("preamble count=%d of %d\n", preambleCount,
        _preambleBitsExpected);
        /*  We expect the preamble to be 6144 bits at the start of the file and
         *  64 bits at the start of each block.  So if longer than 3072 bits we
         *  assume new record.
         */
        if (preambleCount > 3072)
        {
            _recordCount++;

            if (_haveHeader)
            {
                // printf ("*** New record\n");
                _haveHeader = false;
                // blockCount =
                // blockIndex =
                // blockBytes =
                _headerBytes = 0;
            }
        }

        /*  Reset the preamble bit count.  If we have seen at least 60 zero bits in a row we consider this to
         *  be a valid preamble and start reading data.
         */
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

#define GRAPH_WIDTH     100
#define GRAPH_HEIGHT    16
static char graph[GRAPH_WIDTH][GRAPH_HEIGHT];
static int graphCol;

static int graphRange (int x)
{
    x = (x + 24000) /3000;
    if (x<0) x = 0;
    if (x>15) x = 15;
    return x;
}
static void addGraph (int sample, int min, int max, int zero, int state)
{
    if (graphCol == GRAPH_WIDTH)
        return;

    sample = graphRange(sample);
    min = graphRange(min);
    max = graphRange(max);
    zero = graphRange(zero);

    graph[graphCol][min] = 'L';
    graph[graphCol][max] = 'H';
    graph[graphCol][zero] = 'Z';
    graph[graphCol][sample] = '+';
    // graph[graphCol][state?0:15] = '+';
    graphCol++;
}

static void removeGraph (int count)
{
    count++; // remove vert
    if (count > graphCol)
        graphCol = 0;
    else
        graphCol -= count;

    // memset (graph, ' ', GRAPH_HEIGHT*GRAPH_WIDTH);
    for (int y = 0; y < GRAPH_HEIGHT; y++)
        for (int x = 0; x < GRAPH_WIDTH; x++)
        {
            if (x < graphCol)
                graph[x][y] = graph[x+count][y];
            else
                graph[x][y] = ' ';
        }
}

static void addVert (void)
{
    if (graphCol == GRAPH_WIDTH)
        return;
    for (int y = 0; y < GRAPH_HEIGHT; y++)
        graph[graphCol][y] = '|';

    graphCol++;
}

static void drawGraph (void)
{
    for (int y = 0; y < GRAPH_HEIGHT; y++)
    {
        printf ("                        ");

        for (int x = 0; x < graphCol; x++)
            printf ("%c", graph[x][y]);

        printf ("\n");
    }

    // graphCol = 0;
    // memset (graph, ' ', GRAPH_HEIGHT*GRAPH_WIDTH);
}

vector<int> samplesFifo;

// static int fifoSample[64];
// static int fifoHead;

static void fifoAdd (int sample)
{
    if (samplesFifo.size() == 64)
        samplesFifo.erase (samplesFifo.begin());

    // fifoSample[fifoHead++] = sample;
    // fifoHead %= 64;
    samplesFifo.push_back (sample);
}

static int fifoMin (void)
{
    int min = 32767;
    for (auto x : samplesFifo)
        if (x < min)
            min = x;

    return min;
}

static int fifoMax (void)
{
    int max = -32768;
    for (auto x : samplesFifo)
        if (x > max)
            max = x;

    return max;
}

static int fifoGet (void)
{
    return samplesFifo[32];
}

#define SYMBOL_FIFO 5
struct _frame
{
    int width;
    int error;
    bool done;
    char sym;
} ;

vector<struct _frame> frameFifo;

static void frameFifoRemove (int n)
{
    for (int i = 0; i < n; i++)
        if (frameFifo.size() > 0)
            frameFifo.erase(frameFifo.begin());
}

/*  Analyse the width of a half-sine between two zero crossings */
void Tape::inputBitWidth (int width)
{
    // static int bitCount = 0;
    bool haveBit = false;
    int bit = 0;

    struct _frame f;
    f.width = width;
    f.error = 30 - width;
    f.done = false;
    f.sym = '?';
    frameFifo.push_back (f);

    #if 0
    int errsum = 0;
    for (int i = 0; i < 4; i++)
    {
        errsum += lastError[i];
        lastError[i] = lastError[i+1];
    }
    lastError[3] = 30 - width;
    #endif

    if (_showRaw) 
    // printf ("[%d e:%d]\n", width, errsum);
    printf ("[%d,q:%ld]\n", width, frameFifo.size());
    addVert();

    if (frameFifo.size () < 7) // SYMBOL_FIFO)
    {
        if (_showRaw)
        printf ("queued\n");
        return;
    }

    if (_showRaw)
    {
        drawGraph ();
        // addVert();

        printf ("\"");
        for (auto it : frameFifo)
            printf ("%d=%c ", it.width, it.sym);
        printf ("\"");
    }

    int next = 0;
    while (frameFifo[next].done)
        next++;
    
    while (next > 2)
    {
        removeGraph (frameFifo[0].width);
        frameFifoRemove (1);
        next--;
    }
    // printf ("examine frames %d and %d of %ld\n", next, next+1, frameFifo.size());
    int sum = frameFifo[next].width + frameFifo[next+1].width;
    if (sum < 40)
    {
        if (_showRaw)
            printf ("[%d+%d=%d ONE]\n", frameFifo[next].width, frameFifo[next+1].width, sum);

        bit = 1;
        haveBit = true;
        frameFifo[next].done = true;
        frameFifo[next].sym = 'A';
        frameFifo[next].error -= 15;
        frameFifo[next+1].done = true;
        frameFifo[next+1].sym = 'B';
        frameFifo[next+1].error -= 15;
        // removeGraph (frameFifo[1].width);
        // frameFifoRemove (2);
    }
    else if (sum > 56)
    {
        if (_showRaw)
            printf ("[%d+%d=%d ZERO]\n", frameFifo[next].width, frameFifo[next+1].width, sum);

        bit = 0;
        haveBit = true;
        frameFifo[next].done = true;
        frameFifo[next].sym = '0';
        // removeGraph (fifoWidth[0]);
        // framFifoRemove (1);
    }
    else if (frameFifo[next].width > 23)
    {
        if (_showRaw)
            printf ("[%d ZERO]\n", frameFifo[next].width);

        bit = 0;
        haveBit = true;
        frameFifo[next].done = true;
        frameFifo[next].sym = '0';
        // removeGraph (fifoWidth[0]);
        // fifoWidthRemove (1);
    }
    else
    {
        int err = frameFifo[next-1].error;

        int nextFrame = frameFifo[next+1].width + frameFifo[next+2].width;

        if (frameFifo[next+1].width > 30)
        {
            // Next frame is longer than a zero, so we can't be a one, we must be a
            // shortened zero
            bit = 0;
            haveBit = true;
            frameFifo[next].done = true;
            frameFifo[next].sym = '0';
        }
        else if (nextFrame < 45)  // We are undersize, check if next is oversize
        {
            err += 15 - nextFrame;

            if (frameFifo[next].width - err < 22)
            {
                bit = 1;
                haveBit = true;
                frameFifo[next].done = true;
                frameFifo[next].sym = '0';
                frameFifo[next].done = true;
                frameFifo[next].sym = 'A';
                frameFifo[next].error -= 15;
                frameFifo[next+1].done = true;
                frameFifo[next+1].sym = 'B';
                frameFifo[next+1].error -= 15;
            }
            else if (frameFifo[next].width - err > 27)
            {
                // Next frame is a one, are we an undersize zero?
                bit = 0;
                haveBit = true;
                frameFifo[next].done = true;
                frameFifo[next].sym = '0';
            }
        }

        if (!haveBit)
        {
            // no still no luck
            if (_showRaw) 
                printf (" : [%d=???] prev-err=%d nextw=%d new-w=%d\n", frameFifo[next].width,
                        frameFifo[next-1].error, frameFifo[next+1].width,
                        nextFrame);

            frameFifo[next].done = true;
            frameFifo[next].sym = 'X';
            // removeGraph (fifoWidth[0]);
            // fifoWidthRemove (1);
        }
    }

    if (!haveBit)
        return;

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
bool Tape::inputWav (Files *outputFile, const char *inputName, bool showParams)
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

    for (int i = 0; i < 63; i++)
    {
        sample = wav.readSample ();
        fifoAdd (sample);
    }

    memset (graph, ' ', GRAPH_HEIGHT*GRAPH_WIDTH);
    for (int i = 0; i < wav.getSampleCount (); i++)
    {
        // localMax = 0.99 * localMax;  // 0.99 ^ 32 = 0.725
        // localMin = 0.99 * localMin;

        if (i < wav.getSampleCount () - 64)
        {
            sample = wav.readSample ();
            fifoAdd (sample);
        }
        sample = fifoGet ();

        double localMin = fifoMin();
        double localMax = fifoMax();

        double amp = localMax - localMin;
        double zerocross = (localMin+localMax) / 2;
        // Apply hysteresis
        if (state && sample < zerocross - amp * .05)
            state = 0;

        if (!state && sample > zerocross + amp * .05)
            state = 1;

        addGraph (sample, localMin, localMax, zerocross, state);

        if (state == lastState)
        {
            changeCount++;
            continue;
        }

        // if (_showRaw) printf ("(%d,%d,%d)\n", (int) localMin, (int) zerocross, (int) localMax);

        // Found a potential bit, analyse it
        inputBitWidth (changeCount);
        lastState = state;
        changeCount = 0;
    }

    /*  Do a final call to process the width of the last sample */
    inputBitWidth (changeCount);
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

void Tape::outputByte (uint8_t byte)
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

void Tape::outputBlock (uint8_t *data)
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

void Tape::outputWav (uint8_t *inputData, int inputSize)
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

void Tape::showResult ()
{
    printf ("Found %d blocks in %d record%s%s", _blockCount, _recordCount,
            _recordCount > 1 ? "s":"",
            _errorsFound ? "\n" : ", no errors were found\n");

    if (_errorsFound)
        fprintf (stderr, "*** Errors were found which %s\n", 
                 _errorsFixable ? "are fixable" : "can't be fixed");
}

int main (int argc, char *argv[])
{
    Tape tape;
    char c;
    const char *binFileName = "";
    bool create = false;
    bool extract = false;
    bool showWav = false;
    bool verbose = false;
    bool showRaw = false;
    bool tifiles = false;

    while ((c = getopt(argc, argv, "c:e:vrwt")) != -1)
    {
        switch (c)
        {
            case 'c' : create = true; binFileName = optarg; break;
            case 'e' : extract = true; binFileName = optarg; break;
            case 'v' : verbose = true; break;
            case 'r' : showRaw = true; break;
            case 'w' : showWav = true; break;
            case 't' : tifiles = true; break;
            default: printf ("Unknown option '%c'\n", c);
        }
    }

    if (argc - optind < 1)
    {
        printf ("\nTool to read and write wav files for cassette audio\n\n"
                "usage: %s [-c <file>] [-e <file>] [-vrwt] <wav-file>\n"
                "\t where -c=create WAV from <file> (TIFILES or tokenised TI basic)\n"
                "\t       -e=extract to <file>, -v=verbose, -t=add tifiles header,\n"
                "\t       -r=raw bits, -w=show wav hdr\n\n", argv[0]);
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
