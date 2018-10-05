# -Ff=256 -ml -lm # FDT 256, map file, large
bcc -mh -lm cpu.c vdp.c break.c watch.c cond.c cru.c grom.c cover.c
