#ifndef _DEBUGGER_H
#define _DEBUGGER_H

#include <stdbool.h>
#include "mos6510.h"
#include "mem.h"

void debugger_init(void);
bool debugger(mos6510_t *cpu, mem_t *mem);
void debugger_mem_read(uint16_t address);
void debugger_mem_write(uint16_t address, uint8_t value);

#endif /* _DEBUGGER_H */

