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
 *  file and fed to the FM decoder class to be interpreted.
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
#include "fmdecode.h"

class TapeDecode
{
private:
    bool _verbose;
    bool _showRaw;

    bool _errorsFound;
    bool _errorsUnfixable;
    bool _preambleSync;
    int _preambleCount;
    int _preambleStart;
    int _preambleDuration;
    int _preambleBitsExpected;
    double _frameLength;
    int _currentByte;

    bool _haveHeader;
    bool _haveBitTiming;

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
    void inputBitWidth (int count, int position);
    void outputByte (uint8_t byte);
    void outputBlock (uint8_t *data);
public:
    void decodeBit (int bit, int position);
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
            if (_verbose) cout << "Reading " << (int) _header.size1 << " blocks ..." << endl;
            _haveHeader = true;
            _preambleSync = true;
            _preambleCount = 0;
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
        _preambleCount = 0;
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
            _errorsUnfixable = true;
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

void TapeDecode::decodeBit (int bit, int position)
{
    if (bit && _preambleSync)
    {
        if (_showRaw) cout << "preamble count=" << _preambleCount << " of " << _preambleBitsExpected << endl;
        /*  We expect the preamble to be 6144 bits at the start of the file and
         *  64 bits at the start of each block.  So if longer than 3072 bits we
         *  assume new record.  */
        if (_preambleCount > 3072)
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
        if (_preambleCount > _preambleBitsExpected)
        {
            _preambleSync = false;
            _preambleDuration = position - _preambleStart;
        }
        else
        {
            _preambleStart = position;
        }
    }

    if (_preambleSync)
    {
        _preambleCount++;
        return;
    }

    _currentByte <<= 1;
    _currentByte |= bit;
    _bitCount++;

    if (_bitCount == 8)
    {
        if (_showRaw)
            printf ("BYTE %02X (%d/%ld)\n", _currentByte, _blockBytes, sizeof (struct __block));
        decodeBlock (_currentByte);
        _currentByte = 0;
        _bitCount = 0;
    }
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
    {
        if (_errorsUnfixable)
            cerr << "*** Errors were found which can't be fixed automatically but it\n" <<
                    "*** may be possibly to manually correct a hex dump created using -v" << endl;
        else
            cerr << "*** Errors were found which were fixed" << endl;
    }
}

class Decoder : public FMDecoder
{
public:
    void decodeBit (int bit) { tape.decodeBit (bit, 0); }
};

static Decoder decoder;

bool TapeDecode::inputWav (Files *outputFile, const char *inputName, bool showParams)
{
    WavFile wav;

    _file = outputFile;

    if (!wav.openRead (inputName, showParams))
    {
        cerr << "read fail" << endl;
        return false;
    }

    if (_showRaw)
        decoder.showRaw ();

    for (int i = 0; i < wav.getSampleCount (); i++)
    {
        decoder.input (wav.readSample ());
    }

    wav.close ();

    if (!_file->setSize (_blockCount * 64))
    {
        cerr << "resize fail" << endl;
        return false;
    }

    return true;
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
        file.read ();

        if (file.getSize () < 0)
            return 1;

        cassette.fileOpenWrite (argv[optind]);
        tape.outputWav (file.getData (), file.getSize ());
        cassette.fileCloseWrite ();
    }
    else
    {
        Files file (binFileName, tifiles, verbose);
        file.realloc (MAX_PROGRAM_SIZE);
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
