#ifndef _VIC_H
#define _VIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "cia.h"

typedef struct vic_s {
  uint8_t color_ram[1024];
  uint8_t mp;
  uint8_t bg0;
  uint16_t pixel;
  uint16_t raster_line;
  uint16_t raster_compare;
  uint8_t irq_latch;
  uint8_t irq_enable;
  void *cpu;
  void *mem;
} vic_t;

#define VIC_CR1    0x11 /* Control Register 1 */
#define VIC_RASTER 0x12 /* Raster Counter */
#define VIC_MP     0x18 /* Memory Pointers */
#define VIC_IR     0x19 /* Interrupt Register */
#define VIC_IE     0x1A /* Interrupt Enabled */
#define VIC_BG0    0x21 /* Background Color 0 */

uint8_t vic_read_hook(void *vic, uint16_t address);
void vic_write_hook(void *vic, uint16_t address, uint8_t value);
void vic_init(vic_t *vic);
void vic_execute(vic_t *vic);
void vic_dump(FILE *fh, vic_t *vic, cia_t *cia2);

#endif /* _VIC_H */
