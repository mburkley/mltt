all: test_cru test_cpu test_kbd run

test_cru : test_cru.c

test_cpu : test_cpu.c
test_timer : test_timer.c
test_kbd : test_kbd.c

run : test_cru test_cpu test_timer test_kbd
	prove --exec ./test_cru t/
	prove --exec ./test_cpu t/
	prove --exec ./test_timer t/
	prove --exec ./test_kbd t/
