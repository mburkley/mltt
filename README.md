mltt: Mark's Linux Tools for TI99
=================================

This is a collection of tools I use to create and maintain media for my TI99/4A
computer.  It should build on most Linux distros but I have only tested on
Debian and Ubuntu.

To build, just clone from main and make.  libraries etc have to be manually installed.
The emulator depends on GL, glut, pulse-audio and readline libraries.

There is also an experimental web interface [here][1] using a
nodejs backend to execute the CLI.  This supports just cassette tape file
conversions for now.

This project started in C, but any new dev is in C++.  Many of the older files
are just straight C with minimal mods to compile with g++.  I'm using this
project mainly for fun and to "sharpen my sword".

mlt99-emu - TI-99/4A emulator for Linux
---------------------------------------

This is an emulator for the TI-99/4A home computer which was my first PC back in
1982.  I have been writing this emulator on and off for over 20 years.  It still
has some outstanding issues but runs many games perfectly (munchman, ti
invaders, etc).

To run: `./mltt-emu [<config-file>]`

where `<config-file>` is the input file that contains instructions to load
roms/groms/etc.  Default if unspecified is config.txt

Keyboard reads are a bit of a hack.  I read directly from /dev/input to ensure I
get both keyup and keydown events.  The current user must be a member of the
"input" group to read these.  Also note the emulator captures keystrokes even
when it is not the app in focus!

Glut can read the keyboard as well so I plan to use that instead sometime.  But
while it does report key up and down events, it returns ascii codes, not
scancodes, for ordinary keys so some rework is needed.

mlt99-disk - Tool to manage sector dump disk files
--------------------------------------------------

Allows examination of existing disk files, formatting of new disks and
extracting files from disks.

mlt99-tape - Tool to manage CS1 program recordings
--------------------------------------------------

Reads and parses WAV file program recordings.  It can create a binary file
(default) or a new clean WAV file if it was able to correct all errors in the
recording.

See below for more details

mlt99-fuse - Fuse (file system in user space) driver to mount DSK files
-----------------------------------------------------------------------

This tool mounts a sector dump disk image to a specified mount point.  Filenames
are converted from TI to Linux by changing to uppercase and replacing slash (/)
with dot (.).

mlt99-file - Tool to manage TIFILES basic programs, object files and EA5 binaries
---------------------------------------------------------------------------------

This tool can read and write various file formats.  It can add or remove TIFILES
headers, tokenise or detokenise basic and encode / decode DIS/VAR files.


Release Notes
-------------

version 0.93
* Added new methods to cassette WAV read to detect and correct more errors

version 0.92
* Tidied up tools including fuse.
* Added basic tokenise/detokenise and other conversions

version 0.91
* Improved reading of poor quality cassette recordings

version 0.90
* Added alpha lock support (# key toggles)
* Add joystick support (num keypad controls, 0=fire)
* Increase cartidge bank count to 64
* Improvements to bitmap regs
* VDP max 4 sprites per line
* Did some work on Overflow and Odd Parity status flags

FUSE notes
----------
The fuse tool mounts a sector-dump disk image as a directory.  Files are
presented as they are on disk.  I looked at automatically adding a TIFILES
header to every file but there are too many corner cases to make this a viable
option so the the files are not presented with TIFILES headers.  This does mean
there is a possible loss of metadata (record size, etc) when copying to and from
a mounted directory.  To capture this metadata, extended attributes are used
instead.  `mltt-file` can read and write these attributes and can reconstruct a
TIFILES header from them if necessary.

Currently, the extended attributes are just `user.tifiles.flags` and
`user.tifiles.reclen`.  These are checked by the disktool program to find out more
about the file if a TIFILES header is not present.  Other fields such as record
per sector and eof offset are calculated automatically.

Cassette decode implementation notes
------------------------------------
Decoding cassette WAV files has gone through a number of evolutions.  The latest
version introduces timing and local minima/maxima detection.  The success rate
is now pretty good and it can correct errors in all but one of the recordings I
have.

It also dumps (in raw mode) a text graph of a frame when it isn't sure what it
is .  Here is an example:
```
000000000011111111112222222222330000000000111111111122222222223300000000001111111111222222222233
012345678901234567890123456789010123456789012345678901234567890101234567890123456789012345678901
    ++++++++LLLLLLLLLLLLL       |    m                 |             m          |     m
   +        ++   |      mHHHHHHHHHHHH++LLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL          |     m
  +           +  |      m       |  ++m +++             |             m          |     m
 +             + |      m       | +  m    ++           |             m          |     m
+       ZZZZZZZZ+ZZZZZZZZ       |+   m      ++         |             m          |     m
                 +      mZZZZZZZ+ZZZZZ        ++       |             m          |     m
ZZZZZZZZ         |+     m      +|    m          +++    |             mHHHHHHHHHHHHHHH+++++LLLLLZ
                 | +    m     + |    m             ++  |             m          |  ++ m   +++
                 |  ++  m   ++  |    mZZZZZZZZZZZZZZZ++ZZZZZZZZZZZZZZZ          | +   mZZZZZZ+Z
        HHHHHHHHHHHHHH++++++LLLLLLLLLL                 ++            m          |+    mHHHHHHH++
                 |      m       |    m                 | ++          m          +     m
                 |      m       |    m                 |   ++        mZZZZZZZZZZZZZZZZZ
                 |      m       |    m                 |     ++      m         +|     m
                 |      m       |    m                 |       +     m        + |     m
                 |      m       |    m                 |        +    m      ++  |     m
                 |      m       |    mHHHHHHHHHHHHHHHHHHHHHHHHHHH+++++++++++LLLLLLLLLLL
```

This shows the contents of the sample FIFO with an index along the type.  The
FIFO contains 3 frames, prev, curr and next.  Each one is assumed to be 32
samples, so the number along the top goes from 0 to 31 three times.  The
horizontal 'Z' shows the value at which it considers a zero crossing (ZC) to
have occurred.  The vertical bars show where a ZC occurred.  A ZC occurs at the
value which is the mid point between a local minimum and maximum (the vertical
lines of 'm' and the horizontal 'L' and 'H' lines).  Timing corrections attempt
to keep the first ZC at the start of the current sample.

In this frame, the previous was a ONE bit but the current is ambiguous.  There
is a distance of 23 samples between the first and second ZC, which could be half
a ONE bit or a ZERO.   When the next frame is analysed, it will most likely turn
out to have been a ZERO.

[1]: https://www.burkley.net/script
