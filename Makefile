
SRCS=cpu.c \
vdp.c \
break.c \
watch.c \
cond.c \
cru.c \
grom.c \
cover.c \
unasm.c \
kbd.c \
timer.c \
trace.c \
speech.c \
sound.c \
interrupt.c \
gpl.c \
status.c \
cassette.c

LIBS=\
-l glut\
-l GL\
-lpulse-simple\
-lpulse\
-lreadline \
-lm

all:  ti994a tests

ti994a: $(SRCS) console.c ti994a.c
	gcc -Wall -ggdb3 -o ti994a -D__GROM_DEBUG console.c ti994a.c $(SRCS) $(LIBS)

tests: $(SRCS) tests.c
	gcc -Wall -ggdb3 -o tests -D__GROM_DEBUG tests.c $(SRCS) $(LIBS)

testkbd: $(SRCS) kbd.c trace.c
	gcc -Wall -ggdb3 -o testkbd -D__UNIT_TEST kbd.c trace.c $(LIBS)

