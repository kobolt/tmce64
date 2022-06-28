#ifndef _DEBUGGER_H
#define _DEBUGGER_H

#include <stdbool.h>
#include "mos6510.h"
#include "serial_bus.h"
#include "mem.h"

void debugger_init(void);
bool debugger(mos6510_t *cpu, mem_t *mem, serial_bus_t *serial_bus);
void debugger_mem_read(uint16_t address);
void debugger_mem_write(uint16_t address, uint8_t value);
void debugger_mem_execute(uint16_t address);

void debugger_stack_trace_init(void);
void debugger_stack_trace_dump(FILE *fh);
void debugger_stack_trace_add(uint16_t from, uint16_t to);
void debugger_stack_trace_rem(void);

#endif /* _DEBUGGER_H */
