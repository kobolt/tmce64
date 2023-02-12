#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

#include "vic.h"
#include "cia.h"
#include "mos6510.h"



uint8_t vic_read_hook(void *vic, uint16_t address)
{
  switch (address & 0x3F) {
  case VIC_CR1:
    return (((vic_t *)vic)->raster_line >> 8) << 7;

  case VIC_RASTER:
    return ((vic_t *)vic)->raster_line & 0xFF;

  case VIC_MP:
    return ((vic_t *)vic)->mp;

  case VIC_IR:
    return ((vic_t *)vic)->irq_latch & 0xF;

  case VIC_IE:
    return ((vic_t *)vic)->irq_enable & 0xF;

  case VIC_BG0:
    return ((vic_t *)vic)->bg0;

  default:
    return 0;
  }
}



void vic_write_hook(void *vic, uint16_t address, uint8_t value)
{
  switch (address & 0x3F) {
  case VIC_CR1:
    ((vic_t *)vic)->raster_compare =
      (((vic_t *)vic)->raster_compare & 0x00FF) + ((value >> 7) << 8);
    break;

  case VIC_RASTER:
    ((vic_t *)vic)->raster_compare =
      (((vic_t *)vic)->raster_compare & 0xFF00) + value;
    break;

  case VIC_MP:
    ((vic_t *)vic)->mp = value;
    break;

  case VIC_IR:
    ((vic_t *)vic)->irq_latch = value;
    break;

  case VIC_IE:
    ((vic_t *)vic)->irq_enable = value;
    break;

  case VIC_BG0:
    ((vic_t *)vic)->bg0 = value;
    break;

  default:
    break;
  }
}



void vic_init(vic_t *vic)
{
  int i;

  for (i = 0; i < 1024; i++) {
    vic->color_ram[i] = 0xff;
  }

  vic->bg0            = 0;
  vic->mp             = 0;
  vic->pixel          = 0;
  vic->raster_line    = 0;
  vic->raster_compare = 0;
  vic->irq_latch      = 0;
  vic->irq_enable     = 0;

  vic->cpu = NULL;
  vic->mem = NULL;
}



void vic_execute(vic_t *vic)
{
  /* Pixel clock should normally increment by 8, but a value of 6 seems to
     better emulate the VIC-II CPU stunning effect. */
  vic->pixel += 6;
  if (vic->pixel >= 403) {
    vic->pixel = vic->pixel - 403;
    vic->raster_line++;
    if (vic->raster_line >= 284) {
      vic->raster_line = 0;
    }

    /* Raster IRQ */
    if (vic->irq_enable & 0x1) {
      if (vic->raster_line == vic->raster_compare) {
        vic->irq_latch |= 0x1;
        mos6510_irq((mos6510_t *)vic->cpu, (mem_t *)vic->mem);
      }
    }
  }
}



void vic_dump(FILE *fh, vic_t *vic, cia_t *cia2)
{
  int i;

  fprintf(fh, "Memory Pointers: 0x%02x\n", vic->mp);
  fprintf(fh, "  Screen Memory: 0x%04x\n",
    (((vic->mp >> 4) & 0xF) * 0x400) +
    ((~(cia2->data_port_a) & 0x3) * 0x4000));
  fprintf(fh, "  Character Set: 0x%04x\n",
    (((vic->mp >> 1) & 0x7) * 0x800) +
    ((~(cia2->data_port_a) & 0x3) * 0x4000));

  fprintf(fh, "Background Color 0: 0x%02x\n", vic->bg0);

  fprintf(fh, "Raster Line / Pixel: %d/%d\n", vic->raster_line, vic->pixel);
  fprintf(fh, "Raster Compare: %d\n", vic->raster_compare);
  fprintf(fh, "Raster IRQ, Latch: %d, Enable: %d\n",
    vic->irq_latch & 0x1, vic->irq_enable & 0x1);

  fprintf(fh, "Color RAM:\n");
  for (i = 0; i < 1024; i++) {
    if (i % 16 == 0) {
      fprintf(fh, "$%04x   ", i);
    }
    fprintf(fh, "%02x ", vic->color_ram[i]);
    if (i % 4 == 3) {
      fprintf(fh, " ");
    }
    if (i % 16 == 15) {
      fprintf(fh, "\n");
    }
  }
}



