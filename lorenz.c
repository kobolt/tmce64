#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "test.h"
#include "mos6510.h"
#include "mem.h"



static uint8_t petscii_to_ascii[UINT8_MAX + 1] = {
  '.','.','.','.','.','.','.','.','.','.','.','.','.','\n','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '@','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w','x','y','z','[','£',']','.','.',
  '.','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','+','.','|','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','+','.','|','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
};



static void lorenz_test_load(mem_t *mem, const char *filename)
{ 
  FILE *fh;
  int c;
  uint16_t address;
  char path[256];
  
  if (0 == strcmp("trap17", filename)) {
    exit(EXIT_SUCCESS);
  }
  
  snprintf(path, sizeof(path), "%s/%s", LORENZ_TEST_DIRECTORY, filename);
  
  fh = fopen(path, "rb");
  if (fh == NULL) { 
    fprintf(stderr, "Error! Lorenz test file not found: '%s'\n", filename);
    exit(EXIT_FAILURE);
  }
  
  address  = fgetc(fh);
  address += fgetc(fh) * 256;
  
  while ((c = fgetc(fh)) != EOF) {
    mem->ram[address] = c;
    address++; /* Just overflow... */
  }
  
  fclose(fh);
}



static bool lorenz_test_opcode_handler(uint32_t opcode,
  mos6510_t *cpu, mem_t *mem)
{
  uint8_t immediate;
  uint16_t absolute;

  switch (opcode) {
  case 0xFF: /* TRP - Trap! */
    switch (cpu->pc-1) {
    case 0xFFD2: /* Print character */
      mem_write(mem, 0x030C, 0);
      printf("%c", petscii_to_ascii[cpu->a]);
      cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
      cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)) * 256;
      cpu->pc += 1;
      break;

    case 0xE16F: /* Load */
      absolute  = mem_read(mem, 0xBB);
      absolute += mem_read(mem, 0xBC) * 256;
      immediate = mem_read(mem, 0xB7);
      char filename[256];
      int i;
      for (i = 0; i < immediate; i++) {
        filename[i] = petscii_to_ascii[mem_read(mem, absolute + i)];
      }
      filename[i] = '\0';
      lorenz_test_load(mem, filename);
      cpu->sp++;
      cpu->sp++;
      cpu->pc = 0x0816;
      break;

    case 0xFFE4: /* Scan keyboard */
      cpu->a = 3;
      cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
      cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)) * 256;
      cpu->pc += 1;
      break;

    case 0x8000:
    case 0xA474:
      fprintf(stderr, "Exit");
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
 


void lorenz_test_setup(mos6510_t *cpu, mem_t *mem)
{ 
  /* Inject trap opcode */
  mos6510_trap_opcode(0xFF, lorenz_test_opcode_handler);
  
  /* Disable bank switching */
  mem->ram[1] = 0x0;
  
  /* Initial values */
  mem->ram[0x0002] = 0x00;
  mem->ram[0xA002] = 0x00;
  mem->ram[0xA003] = 0x80;
  mem->ram[0xFFFE] = 0x48;
  mem->ram[0xFFFF] = 0xFF;
  mem->ram[0x01FE] = 0xFF;
  mem->ram[0x01FF] = 0x7F;
  
  /* IRQ handler */
  mem->ram[0xFF48] = 0x48;
  mem->ram[0xFF49] = 0x8A;
  mem->ram[0xFF4A] = 0x48;
  mem->ram[0xFF4B] = 0x98;
  mem->ram[0xFF4C] = 0x48;
  mem->ram[0xFF4D] = 0xBA;
  mem->ram[0xFF4E] = 0xBD;
  mem->ram[0xFF4F] = 0x04;
  mem->ram[0xFF50] = 0x01;
  mem->ram[0xFF51] = 0x29;
  mem->ram[0xFF52] = 0x10;
  mem->ram[0xFF53] = 0xF0;
  mem->ram[0xFF54] = 0x03;
  mem->ram[0xFF55] = 0x6C;
  mem->ram[0xFF56] = 0x16;
  mem->ram[0xFF57] = 0x03;
  mem->ram[0xFF58] = 0x6C;
  mem->ram[0xFF59] = 0x14;
  mem->ram[0xFF5A] = 0x03;
  
  /* Trap instructions */
  mem->ram[0xFFD2] = 0xFF;
  mem->ram[0xE16F] = 0xFF;
  mem->ram[0xFFE4] = 0xFF;
  mem->ram[0x8000] = 0xFF;
  mem->ram[0xA474] = 0xFF;
  
  /* Load initial test */
  lorenz_test_load(mem, " start");
  
  /* Initial CPU settings */
  cpu->sp = 0xFD;
  cpu->sr.i = 1;
  cpu->pc = 0x0801;
}



