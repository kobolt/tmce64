#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

#include "mem.h"
#include "debugger.h"



static uint8_t io_read(mem_t *mem, uint16_t address)
{
  if (address >= 0xDF00) { /* I/O #2 */
    /* Not implemented. */

  } else if (address >= 0xDE00) { /* I/O #1 */
    /* Not implemented. */

  } else if (address >= 0xDD00) { /* CIA #2 */
    if (mem->cia2 != NULL && mem->cia_read != NULL) {
      return (mem->cia_read)(mem->cia2, address);
    }

  } else if (address >= 0xDC00) { /* CIA #1 */
    if (mem->cia1 != NULL && mem->cia_read != NULL) {
      return (mem->cia_read)(mem->cia1, address);
    }

  } else if (address >= 0xD800) { /* Color RAM */
    return mem->color_ram[address - 0xD800];

  } else if (address >= 0xD400) { /* SID */
    /* Not implemented. */

  } else if (address >= 0xD000) { /* VIC-II */
    if (address == 0xD021) { /* Background colour 0 */
      return mem->vic2_bg0;
    } else if (address == 0xD018) { /* Memory pointers */
      return mem->vic2_mp;
    }
  }
  return 0;
}



static void io_write(mem_t *mem, uint16_t address, uint8_t value)
{
  if (address >= 0xDF00) { /* I/O #2 */
    /* Not implemented. */

  } else if (address >= 0xDE00) { /* I/O #1 */
    /* Not implemented. */

  } else if (address >= 0xDD00) { /* CIA #2 */
    if (mem->cia2 != NULL && mem->cia_write != NULL) {
      (mem->cia_write)(mem->cia2, address, value);
    }

  } else if (address >= 0xDC00) { /* CIA #1 */
    if (mem->cia1 != NULL && mem->cia_write != NULL) {
      (mem->cia_write)(mem->cia1, address, value);
    }

  } else if (address >= 0xD800) { /* Color RAM */
    mem->color_ram[address - 0xD800] = value;

  } else if (address >= 0xD400) { /* SID */
    /* Not implemented. */

  } else if (address >= 0xD000) { /* VIC-II */
    if (address == 0xD021) { /* Background colour 0 */
      mem->vic2_bg0 = value;
    } else if (address == 0xD018) { /* Memory pointers */
      mem->vic2_mp = value;
    }
  }
}



void mem_init(mem_t *mem)
{
  int i;

  /* Initialize all to 0xFF since 0x00 is the opcode for BRK. */
  for (i = 0; i <= UINT16_MAX; i++) {
    mem->ram[i] = 0xff;
  }
  for (i = 0; i <= UINT16_MAX; i++) {
    mem->rom[i] = 0xff;
  }
  for (i = 0; i < 1024; i++) {
    mem->color_ram[i] = 0xff;
  }

  /* Internal VIC-II registers. */
  mem->vic2_bg0 = 0;
  mem->vic2_mp = 0;

  /* CIA connection. */
  mem->cia_read = NULL;
  mem->cia_write = NULL;
  mem->cia1 = NULL;
  mem->cia2 = NULL;

  /* Setup I/O registers in the zero page to default. */
  mem->ram[0] = 0b00000000; /* All inputs! */
  mem->ram[1] = 0b00111111;
}



uint8_t mem_read(mem_t *mem, uint16_t address)
{
  debugger_mem_read(address);

  if (address >= 0xA000 && address <= 0xBFFF) {
    if ((mem->ram[1] & MEM_LORAM) && (mem->ram[1] & MEM_HIRAM)) {
      return mem->rom[address]; /* BASIC ROM */
    }
  }
  if (address >= 0xD000 && address <= 0xDFFF) {
    if ((mem->ram[1] & MEM_LORAM) || (mem->ram[1] & MEM_HIRAM)) {
      if (mem->ram[1] & MEM_CHAREN) {
        return io_read(mem, address);
      } else {
        return mem->rom[address]; /* CHAR ROM */
      }
    }
  }
  if (address >= 0xE000 && address < 0xFFFF) {
    if (mem->ram[1] & MEM_HIRAM) {
      return mem->rom[address]; /* KERNAL ROM */
    }
  }

  return mem->ram[address];
}



void mem_write(mem_t *mem, uint16_t address, uint8_t value)
{
  debugger_mem_write(address, value);

  if (address >= 0xD000 && address <= 0xDFFF) {
    if ((mem->ram[1] & MEM_LORAM) || (mem->ram[1] & MEM_HIRAM)) {
      if (mem->ram[1] & MEM_CHAREN) {
        return io_write(mem, address, value);
      }
    }
  }

  mem->ram[address] = value;
}



int mem_load_rom(mem_t *mem, const char *filename, uint16_t address)
{
  FILE *fh;
  int c;

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    return -1;
  }

  while ((c = fgetc(fh)) != EOF) {
    mem->rom[address] = c;
    address++; /* Just overflow... */
  }

  fclose(fh);
  return 0;
}



int mem_load_prg(mem_t *mem, const char *filename)
{
  FILE *fh;
  uint16_t address;
  uint16_t end;
  int c;

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    return -1;
  }

  /* First two bytes specify the loading address. */
  address  = fgetc(fh);
  address += fgetc(fh) * 256;

  end = address;
  while ((c = fgetc(fh)) != EOF) {
    mem->ram[address] = c;
    address++; /* Just overflow... */
    end++;
  }

  /* $002D-$002E = Pointer to beginning of variable area. */
  mem->ram[0x2D] = end % 256;
  mem->ram[0x2E] = end / 256;
  /* $002F-$0030 = Pointer to beginning of array variable area. */
  mem->ram[0x2F] = end % 256;
  mem->ram[0x30] = end / 256;
  /* $0031-$0032 = Pointer to end of array variable area. */
  mem->ram[0x31] = end % 256;
  mem->ram[0x32] = end / 256;

  fclose(fh);
  return 0;
}



static void mem_ram_dump_16(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  uint16_t address;

  fprintf(fh, "$%04x   ", start & 0xFFF0);

  /* Hex */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    if ((address >= start) && (address <= end)) {
      fprintf(fh, "%02x ", mem->ram[address]);
    } else {
      fprintf(fh, "   ");
    }
    if (i % 4 == 3) {
      fprintf(fh, " ");
    }
  }

  /* Character */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    if ((address >= start) && (address <= end)) {
      if (isprint(mem->ram[address])) {
        fprintf(fh, "%c", mem->ram[address]);
      } else {
        fprintf(fh, ".");
      }
    } else {
      fprintf(fh, " ");
    }
  }

  fprintf(fh, "\n");
}



void mem_ram_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  mem_ram_dump_16(fh, mem, start, end);
  for (i = (start & 0xFFF0) + 16; i <= end; i += 16) {
    mem_ram_dump_16(fh, mem, i, end);
  }
}



