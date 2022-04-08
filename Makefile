CFLAGS=-Wall -Wextra -lncurses

all: tmce64

tmce64: main.o mos6510.o mos6510_trace.o mem.o cia.o console.o debugger.o lorenz.o dormann.o
	gcc -o tmce64 $^ ${CFLAGS}

main.o: main.c
	gcc -c $^ ${CFLAGS}

mos6510.o: mos6510.c
	gcc -c $^ ${CFLAGS}

mos6510_trace.o: mos6510_trace.c
	gcc -c $^ ${CFLAGS}

mem.o: mem.c
	gcc -c $^ ${CFLAGS}

cia.o: cia.c
	gcc -c $^ ${CFLAGS}

console.o: console.c
	gcc -c $^ ${CFLAGS}

debugger.o: debugger.c
	gcc -c $^ ${CFLAGS}

lorenz.o: lorenz.c
	gcc -c $^ ${CFLAGS}

dormann.o: dormann.c
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o tmce64

