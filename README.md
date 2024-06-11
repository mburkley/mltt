mltt: Mark's Linux Tools for TI99
=================================

This is a collection of tools I use to create and maintain media for my TI99/4A
computer.  It should build on most Linux distros but I have only tested on
Debian and Ubuntu.

To build, just clone from main and make.  libraries etc have to be manually installed.
The emulator depends on GL, glut, pulse-audio and readline libraries.

mlt99-emu - TI-99/4A emulator for Linux
---------------------------------------

This is an emulator for the TI-99/4A home computer which was my first PC back in
1982.  I have been writing this emulator on and off for over 20 years.  It still
has some outstanding issues but runs many games perfectly (munchman, ti
invaders, etc).

To run: ./mltt-emu [<config-file>]

where <config-file> is the input file that contains instructions to load
roms/groms/etc.  Default if unspecified is config.txt

Keyboard reads are a bit of a hack.  I read directly from /dev/input to ensure I
get both keyup and keydown events.  The current user must be a member of the
"input" group to read these.  Also note the emulator captures keystrokes even
when it is not the app in focus!

mlt99-disk - Tool to manage sector dump disk files
--------------------------------------------------

Allows examination of existing disk files, formatting of new disks and
extracting files from disks.

mlt99-tape - Tool to manage CS1 program recordings
--------------------------------------------------

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
a mounted directory.  To capture the metadata, extended attributes are used
instead.  mltt-file can read and write these attributes and can reconstruct a
TIFILES header from them if necessary.

Instead files presented by fuse have extended attributes.  These are user.tifiles.flags
and user.tifiles.reclen.  These are checked by the disktool program to find out
more about the file if a TIFILES header is not present.  Other fields such as
record per sector and eof offset are calculated automatically.

