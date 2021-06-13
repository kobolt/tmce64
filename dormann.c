#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "test.h"
#include "cpu.h"
#include "mem.h"



static void dormann_test_load(mem_t *mem, const char *filename)
{
  FILE *fh;
  int c;
  uint16_t address = 0x0;

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    fprintf(stderr, "Error! Dormann test file not found: %s\n", filename);
    exit(1);
  }

  while ((c = fgetc(fh)) != EOF) {
    mem->ram[address] = c;
    address++; /* Just overflow... */
  }

  fclose(fh);
}



void dormann_test_setup(cpu_t *cpu, mem_t *mem)
{
  dormann_test_load(mem, DORMANN_TEST_FILE);
  mem->ram[1] = 0x0; /* Disable bank switching. */
  cpu->pc = 0x0400;
}



