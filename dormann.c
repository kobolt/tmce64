#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "test.h"
#include "mos6510.h"
#include "mem.h"

#define TRAP_OPCODE 0x02



static void dormann_test_load(mem_t *mem, const char *filename)
{
  FILE *fh;
  int c;
  uint16_t address = 0x0;

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    fprintf(stderr, "Error! Dormann test file not found: %s\n", filename);
    exit(EXIT_FAILURE);
  }

  while ((c = fgetc(fh)) != EOF) {
    mem->ram[address] = c;
    address++; /* Just overflow... */
  }

  fclose(fh);
}



static bool dormann_test_opcode_handler(uint32_t opcode,
  mos6510_t *cpu, mem_t *mem)
{
  (void)mem;

  switch (opcode) {
  case TRAP_OPCODE:
    switch (cpu->pc-1) {
    case 0x3469: /* Test success */
      fprintf(stderr, "Test completed successfully.\n");
      exit(EXIT_SUCCESS);
      break;

    default:
      fprintf(stderr, "Error! Unhandled trap: %04X\n", cpu->pc-1);
      exit(EXIT_FAILURE);
      break;
    }
    return true;

  default:
    return false;
  }
}



void dormann_test_setup(mos6510_t *cpu, mem_t *mem)
{
  dormann_test_load(mem, DORMANN_TEST_FILE);
  mem->ram[1] = 0x0; /* Disable bank switching. */
  cpu->pc = 0x0400;

  mem->ram[0x3469] = TRAP_OPCODE; /* Inject trap code on end of test. */
  mos6510_trap_opcode(TRAP_OPCODE, dormann_test_opcode_handler);
}



