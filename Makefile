
OBJECTS=cpu.o \
vdp.o \
break.o \
watch.o \
cond.o \
cru.o \
grom.o \
unasm.o \
kbd.o \
timer.o \
trace.o \
speech.o \
sound.o \
interrupt.o \
gpl.o \
status.o \
cassette.o \
parse.o \
mem.o \
disk.o \
diskfile.o \
diskdir.o \
sams.o \
wav.o \
files.o \
tibasic_encode.o \
tibasic_decode.o

LIBS=\
-l glut\
-l GL\
-lpulse-simple\
-lpulse\
-lreadline \
-lm 

TOOLS=disasm tapetool disktool hexed fir filetool tibasic

CFLAGS=-Wall -ggdb3 -DVERSION=`cat VERSION`
# LDFLAGS=

all:  ti994a tests $(TOOLS)

ti994a: $(OBJECTS) console.o ti994a.o
	@echo "\t[LD] $@..."
	@$(CC) $(LDFLAGS) -o $@ console.o ti994a.o $(OBJECTS) $(LIBS)

tests: $(OBJECTS) tests.o
	@echo "\t[LD] $@..."
	@$(CC) $(LDFLAGS) -o $@ tests.o $(OBJECTS) $(LIBS)

$(TOOLS): %: $(OBJECTS) %.o
	@echo "\t[LD] $@..."
	@$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	@echo "\t[CC] $<..."
	@$(CC) -c $(CFLAGS) $< -o $@
