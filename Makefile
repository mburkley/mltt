
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
fdd.o \
fddfile.o \
diskfile.o \
diskvolume.o \
disksector.o \
sams.o \
wav.o \
files.o \
tibasic_encode.o \
tibasic_decode.o \
file_disvar.o \
textgraph.o \
fmdecode.o

LIBS=\
-l glut\
-l GL\
-lpulse-simple\
-lpulse\
-lreadline \
-lm \
-lfuse3

TOOLS=mltt-disasm mltt-tape mltt-disk mltt-file mltt-fuse

CFLAGS=-Wall -ggdb3 -DVERSION=`cat VERSION` -I/usr/include/fuse3
# LDFLAGS=

all:  mltt-emu $(TOOLS)

mltt-emu: $(OBJECTS) console.o ti994a.o
	@echo "\t[LD] $@..."
	@$(CXX) $(LDFLAGS) -o $@ console.o ti994a.o $(OBJECTS) $(LIBS)

$(TOOLS): %: $(OBJECTS) %.o
	@echo "\t[LD] $@..."
	@$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

# %.o: %.c
# 	@echo "\t[CC] $<..."
# 	@$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cc
	@echo "\t[CC] $<..."
	@$(CXX) -c $(CFLAGS) $< -o $@
