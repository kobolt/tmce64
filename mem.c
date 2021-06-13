#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "mem.h"



static mem_read_hook_t mem_ram_read_hook = NULL;
static mem_write_hook_t mem_ram_write_hook = NULL;



static uint8_t io_read(uint16_t address)
{
  /* Not implemented. */
  return 0;
}



static void io_write(uint16_t address, uint8_t value)
{
  /* Not implemented. */
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

  /* Setup I/O registers in the zero page to default. */
  mem->ram[0] = 0b00000000; /* All inputs! */
  mem->ram[1] = 0b00111111;
}



uint8_t mem_read(mem_t *mem, uint16_t address)
{
  uint8_t value;

  if (address >= 0xA000 && address <= 0xBFFF) {
    if ((mem->ram[1] & MEM_LORAM) && (mem->ram[1] & MEM_HIRAM)) {
      return mem->rom[address]; /* BASIC ROM */
    }
  }
  if (address >= 0xD000 && address <= 0xDFFF) {
    if ((mem->ram[1] & MEM_LORAM) || (mem->ram[1] & MEM_HIRAM)) {
      if (mem->ram[1] & MEM_CHAREN) {
        return io_read(address);
      } else {
        return mem->rom[address]; /* CHAR ROM */
      }
    }
  }
  if (address >= 0xE000 && address <= 0xFFFF) {
    if (mem->ram[1] & MEM_HIRAM) {
      return mem->rom[address]; /* KERNAL ROM */
    }
  }

  if (mem_ram_read_hook != NULL) {
    if (true == (mem_ram_read_hook)(mem, address, &value)) {
      return value;
    }
  }

  return mem->ram[address];
}



void mem_write(mem_t *mem, uint16_t address, uint8_t value)
{
  if (address >= 0xD000 && address <= 0xDFFF) {
    if ((mem->ram[1] & MEM_LORAM) || (mem->ram[1] & MEM_HIRAM)) {
      if (mem->ram[1] & MEM_CHAREN) {
        return io_write(address, value);
      }
    }
  }

  if (mem_ram_write_hook != NULL) {
    if (true == (mem_ram_write_hook)(mem, address, value)) {
      return;
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



void mem_ram_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  for (i = start; i <= end; i++) {
    if (i % 16 == 0) {
      fprintf(fh, "$%04x   ", i);
    }
    fprintf(fh, "%02x ", mem->ram[i]);
    if (i % 16 == 15) {
      fprintf(fh, "\n");
    }
  }
}



void mem_ram_read_hook_install(mem_read_hook_t hook)
{
  mem_ram_read_hook = hook;
}

void mem_ram_write_hook_install(mem_write_hook_t hook)
{
  mem_ram_write_hook = hook;
}



