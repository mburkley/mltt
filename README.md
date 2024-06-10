# ti994a
TI-99/4A emulator for Linux
===========================

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

Release Notes
-------------

version 0.90
* Added alpha lock support (# key toggles)
* Add joystick support (num keypad controls, 0=fire)
* Increase cartidge bank count to 64
* Improvements to bitmap regs
* VDP max 4 sprites per line
* Did some work on Overflow and Odd Parity status flags

FUSE
----
The fuse tool mounts a sector-dump disk image as a directory.  Files are
presented as they are on disk by default.  Optionally, if the -t flag is
provided when fue is invoked, then all files will also have a 128-byte "TIFILES"
header.

Files presented by fuse have extended attributes.  These are user.tifiles.flags
and user.tifiles.reclen.  These are checked by the disktool program to find out
more about the file if a TIFILES header is not present.

Copying files into a fuse mounted directory causes a challenge when files may or
may not have a tifiles header.  The following algorithm is used.  If the start
of the file has the signature of a TIFILES header, then the first 128 bytes are
not copied to the disk image but the meta-data from the header is used to
construct the directory entry in the DSK image.  A flag is maintained internally to record that
this file does not have a TIFILES header.  The flags and record len default to
0.  Reading back the file will not provide a header even if the volume has been
mounted with -t.  This is to ensure that a read of a file is consistent with the
data that was written.

If the file system is unmounted and remounted with -t then a TIFILES header will
be added to all files.

If the -d (decode) option is provided, then instead of the file on disk being
presented, a decoded copy is presented instead.  If the file is detected as a
basic program then detokenised basic is presented.  If the file is detected as a
DIS/VAR file then text is presented.  When a file has been written and is
closed, it is re-encoded before being stored to the file system.

