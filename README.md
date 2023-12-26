# ti994a
TI-99/4A emulator for Linux

This is an emulator for the TI-99/4A home computer which was my first PC back in
1982.  I have been writing this emulator on and off for over 20 years.  It still
has some outstanding issues but runs many games perfectly (munchman, ti
invaders, etc).

To build, download and type make.  libraries etc have to be manually installed.
The emulator depends on GL, glut, pulse-audio and readline libraries.

To run: ./ti994a <config-file>

where <config-file> is the input file that contains instructions to load
roms/groms/etc.  Default if unspecified is config.txt

There are also command line tools to read/write cassette audio files, decode
basic from tifiles and list or create sector dump disk files.

