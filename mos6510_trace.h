#ifndef _MOS6510_TRACE_H
#define _MOS6510_TRACE_H

#include <stdio.h>
#include "mos6510.h"
#include "mem.h"

void mos6510_trace_init(void);
void mos6510_trace_add(mos6510_t *cpu, mem_t *mem);
void mos6510_trace_dump(FILE *fh);

#endif /* _MOS6510_TRACE_H */
