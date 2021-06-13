CFLAGS=-Wall

all: tmce64

tmce64: main.o cpu.o mem.o lorenz.o dormann.o
	gcc -o tmce64 $^ ${CFLAGS}

main.o: main.c
	gcc -c $^ ${CFLAGS}

cpu.o: cpu.c
	gcc -c $^ ${CFLAGS}

mem.o: mem.c
	gcc -c $^ ${CFLAGS}

lorenz.o: lorenz.c
	gcc -c $^ ${CFLAGS}

dormann.o: dormann.c
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o tmce64

