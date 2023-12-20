
SRCS=cpu.c \
vdp.c \
break.c \
watch.c \
cond.c \
cru.c \
grom.c \
unasm.c \
kbd.c \
timer.c \
trace.c \
speech.c \
sound.c \
interrupt.c \
gpl.c \
status.c \
cassette.c \
parse.c \
mem.c \
disk.c \
diskfile.c \
diskdir.c \
sams.c \
wav.c \
files.c \
decodebasic.c

LIBS=\
-l glut\
-l GL\
-lpulse-simple\
-lpulse\
-lreadline \
-lm \
-lfftw3

all:  ti994a tests tapetool disktool unasm hexed fir

ti994a: $(SRCS) console.c ti994a.c
	gcc -Wall -ggdb3 -o ti994a -D__GROM_DEBUG console.c ti994a.c $(SRCS) $(LIBS)

tests: $(SRCS) tests.c
	gcc -Wall -ggdb3 -o tests -D__GROM_DEBUG tests.c $(SRCS) $(LIBS)

unasm: $(SRCS) unasm.c
	gcc -Wall -ggdb3 -o unasm -D__BUILD_UNASM ti994a.c $(SRCS) $(LIBS)

testkbd: $(SRCS) kbd.c trace.c
	gcc -Wall -ggdb3 -o testkbd -D__UNIT_TEST kbd.c trace.c $(LIBS)

tapetool: $(SRCS) tapetool.c decodebasic.c
	gcc -Wall -ggdb3 -o tapetool tapetool.c $(SRCS) $(LIBS)
disktool: $(SRCS) disktool.c decodebasic.c
	gcc -Wall -ggdb3 -o disktool disktool.c $(SRCS) $(LIBS)
hexed: hexed.c parse.c
	gcc -Wall -ggdb3 -o hexed hexed.c parse.c
fir: $(SRCS) fir.c
	gcc -Wall -ggdb3 -o fir fir.c $(SRCS) $(LIBS)
