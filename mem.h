#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct mem_s {
  uint8_t ram[UINT16_MAX + 1];
  uint8_t rom[UINT16_MAX + 1];
} mem_t;

#define MEM_PAGE_STACK 0x100

#define MEM_VECTOR_RESET_LOW  0xFFFC
#define MEM_VECTOR_RESET_HIGH 0xFFFD
#define MEM_VECTOR_IRQ_LOW    0xFFFE
#define MEM_VECTOR_IRQ_HIGH   0xFFFF

#define MEM_LORAM  0b001
#define MEM_HIRAM  0b010
#define MEM_CHAREN 0b100

typedef bool (*mem_read_hook_t)(mem_t *, uint16_t, uint8_t *);
typedef bool (*mem_write_hook_t)(mem_t *, uint16_t, uint8_t);

void mem_init(mem_t *mem);
uint8_t mem_read(mem_t *mem, uint16_t address);
void mem_write(mem_t *mem, uint16_t address, uint8_t value);
int mem_load_rom(mem_t *mem, const char *filename, uint16_t address);
void mem_ram_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end);

void mem_ram_read_hook_install(mem_read_hook_t hook);
void mem_ram_write_hook_install(mem_write_hook_t hook);

#endif /* _MEM_H */
