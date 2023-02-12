
RESID_LIB_PATH=../resid/lib/
RESID_INC_PATH=../resid/inc/

OBJECTS=main.o mos6510.o mos6510_trace.o mem.o cia.o vic.o serial_bus.o disk.o console.o joystick.o debugger.o lorenz.o dormann.o
CFLAGS=-Wall -Wextra
LDFLAGS=-lncurses -lSDL2

ifneq (,$(wildcard ${RESID_LIB_PATH}/libresid.a))
# reSID found!
CFLAGS+=-I${RESID_INC_PATH} -DRESID
LDFLAGS+=-lresid -L${RESID_LIB_PATH} -lm -lstdc++
OBJECTS+=resid.o
endif

all: tmce64

tmce64: ${OBJECTS}
	gcc -o tmce64 $^ ${LDFLAGS}

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

vic.o: vic.c
	gcc -c $^ ${CFLAGS}

serial_bus.o: serial_bus.c
	gcc -c $^ ${CFLAGS}

disk.o: disk.c
	gcc -c $^ ${CFLAGS}

console.o: console.c
	gcc -c $^ ${CFLAGS}

joystick.o: joystick.c
	gcc -c $^ ${CFLAGS}

debugger.o: debugger.c
	gcc -c $^ ${CFLAGS}

lorenz.o: lorenz.c
	gcc -c $^ ${CFLAGS}

dormann.o: dormann.c
	gcc -c $^ ${CFLAGS}

resid.o: resid.cpp
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o tmce64

