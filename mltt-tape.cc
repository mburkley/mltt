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
    Tape () { memset (this, 0, sizeof (*this)); }
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
            _preambleBitsExpected = 60;
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
        _preambleBitsExpected = 60;
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
    static int bitCount;
    static int preambleCount;

    if (bit && _preambleSync)
    {
        if (_showRaw) printf ("preamble count=%d\n", preambleCount);
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
    bitCount++;

    if (bitCount == 8)
    {
        if (_showRaw)
            printf (" %02X (%d/%ld)\n", byte, _blockBytes, sizeof (struct __block));
        decodeBlock (byte);
        byte = 0;
        bitCount = 0;
    }
}

void Tape::inputBitWidth (int count)
{
    static int halfBit = 0;
    if (_showRaw) printf("[%d]", count);
    /*  A peak for a 0 occurs at around 32 samples, for a 1 it is
     *  around 16 samples.  Select 24 as the cutoff to differentiate a 0
     *  from a 1.
     */
    int bit = (count < 24);
    if (bit)
    {
        halfBit++;

        if (halfBit == 2)
        {
            if (_showRaw) printf ("1");

            decodeBit (1);
            halfBit = 0;
        }
    }
    else
    {
        if (_showRaw) printf ("0");
        decodeBit (0);
        halfBit = 0;
    }
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
    int lastState = 0;
    double localMin = 0;
    double localMax = 0;

    _file = outputFile;

    if (!wav.openRead (inputName, showParams))
    {
        cerr << "read fail" << endl;
        return false;
    }

    for (int i = 0; i < wav.getSampleCount (); i++)
    {
        localMax = 0.8 * localMax;  // 0.99 ^ 32 = 0.725
        localMin = 0.8 * localMin;
        sample = wav.readSample ();

        if (sample > localMax)
            localMax = sample;

        if (sample < localMin)
            localMin = sample;

        // Apply hysteresis
        if (state && sample > 0.8 * localMax)
            state = 0;

        if (!state && sample < 0.8 * localMin)
            state = 1;

        if (state == lastState)
        {
            changeCount++;
            continue;
        }

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
