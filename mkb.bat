# -Ff=256 -ml -lm # FDT 256, map file, large
bcc -mh -lm -D__UNIT_TEST mem.c vdp.c grom.c
tlink @turboc.$ln
bcc -mh -lm -D__GROM_DEBUG cpu.c vdp.c break.c watch.c cond.c cru.c grom.c cover.c unasm.c mem.c
tlink @turboc.$ln
