#ifndef _TEST_H
#define _TEST_H

#include "mos6510.h"
#include "mem.h"

#define LORENZ_TEST_DIRECTORY "./lorenz"
#define DORMANN_TEST_FILE "./dormann/6502_functional_test.bin"

void lorenz_test_setup(mos6510_t *cpu, mem_t *mem);
void dormann_test_setup(mos6510_t *cpu, mem_t *mem);

#endif /* _TEST_H */
