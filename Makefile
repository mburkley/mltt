test: mem.c vdp.c grom.c
	gcc -o test -D__UNIT_TEST mem.c vdp.c grom.c

grom: cpu.c vdp.c break.c watch.c cond.c cru.c grom.c cover.c unasm.c mem.c
	gcc -o grom -D__GROM_DEBUG cpu.c vdp.c break.c watch.c cond.c cru.c grom.c cover.c unasm.c mem.c

