#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef uint8_t (*mem_read_hook_t)(void *, uint16_t);
typedef void (*mem_write_hook_t)(void *, uint16_t, uint8_t);

typedef struct mem_s {
  uint8_t ram[UINT16_MAX + 1];
  uint8_t rom[UINT16_MAX + 1];
  uint8_t color_ram[1024];
  uint8_t vic2_bg0;
  uint8_t vic2_mp;
  mem_read_hook_t  cia_read;
  mem_write_hook_t cia_write;
  void *cia1;
  void *cia2;
} mem_t;

#define MEM_PAGE_STACK 0x100

#define MEM_LORAM  0b001
#define MEM_HIRAM  0b010
#define MEM_CHAREN 0b100

void mem_init(mem_t *mem);
uint8_t mem_read(mem_t *mem, uint16_t address);
void mem_write(mem_t *mem, uint16_t address, uint8_t value);
int mem_load_rom(mem_t *mem, const char *filename, uint16_t address);
int mem_load_prg(mem_t *mem, const char *filename);
void mem_ram_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end);
void mem_vic2_dump(FILE *fh, mem_t *mem);

#endif /* _MEM_H */
