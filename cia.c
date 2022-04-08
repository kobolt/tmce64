#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "cia.h"
#include "mos6510.h"



static uint8_t bcd(uint8_t value)
{
  return ((value / 10) * 0x10) + value % 10;
}



uint8_t cia_read_hook(void *cia, uint16_t address)
{
  uint8_t value;
  struct timespec tp;
  struct tm tm;

  switch (address & 0xF) {
  case CIA_TA_LO:
    return ((cia_t *)cia)->timer_a.counter & 0xFF;

  case CIA_TA_HI:
    return ((cia_t *)cia)->timer_a.counter >> 8;

  case CIA_TB_LO:
    return ((cia_t *)cia)->timer_b.counter & 0xFF;

  case CIA_TB_HI:
    return ((cia_t *)cia)->timer_b.counter >> 8;

  case CIA_TOD_10THS:
  case CIA_TOD_SEC:
  case CIA_TOD_MIN:
  case CIA_TOD_HR:
    /* Return the actual host system time! */
    clock_gettime(CLOCK_REALTIME, &tp);
    localtime_r(&tp.tv_sec, &tm);
    if ((address & 0xF) == CIA_TOD_10THS) {
      return tp.tv_nsec / 100000000;
    } else if ((address & 0xF) == CIA_TOD_SEC) {
      return bcd(tm.tm_sec);
    } else if ((address & 0xF) == CIA_TOD_MIN) {
      return bcd(tm.tm_min);
    } else if ((address & 0xF) == CIA_TOD_HR) {
      if (tm.tm_hour <= 12) {
        return bcd(tm.tm_hour); /* AM */
      } else {
        return bcd(tm.tm_hour % 12) + 0x80; /* PM */
      }
    }
    return 0;

  case CIA_ICR:
    value = ((cia_t *)cia)->icr_status;
    ((cia_t *)cia)->icr_status = 0; /* Clear on read. */
    return value;

  case CIA_CRA:
    return ((cia_t *)cia)->timer_a.control;

  case CIA_CRB:
    return ((cia_t *)cia)->timer_b.control;

  default:
    return 0;
  }
}



void cia_write_hook(void *cia, uint16_t address, uint8_t value)
{
  switch (address & 0xF) {
  case CIA_TA_LO:
    ((cia_t *)cia)->timer_a.latch = 
      (((cia_t *)cia)->timer_a.latch & 0xFF00) | value;
    break;

  case CIA_TA_HI:
    ((cia_t *)cia)->timer_a.latch = 
      (((cia_t *)cia)->timer_a.latch & 0x00FF) | (value << 8);
    /* Load counter if not running. */
    if ((((cia_t *)cia)->timer_a.control & 0x1) == 0) {
      ((cia_t *)cia)->timer_a.counter = ((cia_t *)cia)->timer_a.latch;
    }
    break;

  case CIA_TB_LO:
    ((cia_t *)cia)->timer_b.latch = 
      (((cia_t *)cia)->timer_b.latch & 0xFF00) | value;
    break;

  case CIA_TB_HI:
    ((cia_t *)cia)->timer_b.latch = 
      (((cia_t *)cia)->timer_b.latch & 0x00FF) | (value << 8);
    /* Load counter if not running. */
    if ((((cia_t *)cia)->timer_b.control & 0x1) == 0) {
      ((cia_t *)cia)->timer_b.counter = ((cia_t *)cia)->timer_b.latch;
    }
    break;

  case CIA_ICR:
    if (value & 0x80) { /* Set Mask */
      ((cia_t *)cia)->icr_mask |= (value & 0x1F);
    } else { /* Clear Mask */
      ((cia_t *)cia)->icr_mask &= ~(value & 0x1F);
    }
    break;

  case CIA_CRA:
    ((cia_t *)cia)->timer_a.control = value;
    break;

  case CIA_CRB:
    ((cia_t *)cia)->timer_b.control = value;
    break;

  default:
    break;
  }
}



void cia_init(cia_t *cia, int cia_no)
{
  cia->no = cia_no;
  cia->icr_status = 0;
  cia->icr_mask = 0;
  cia->timer_a.control = 0;
  cia->timer_b.control = 0;
  cia->timer_a.latch = 0xFFFF;
  cia->timer_b.latch = 0xFFFF;
  cia->timer_a.counter = 0xFFFF;
  cia->timer_b.counter = 0xFFFF;
}



void cia_execute(cia_t *cia)
{
  /* NOTE: Both timers just run at the system/CPU clock. */

  if (cia->timer_a.control & 0x10) { /* Timer A Force Load */
    cia->timer_a.counter = cia->timer_a.latch;
    cia->timer_a.control &= ~0x10;
  }
  if (cia->timer_b.control & 0x10) { /* Timer B Force Load */
    cia->timer_b.counter = cia->timer_b.latch;
    cia->timer_b.control &= ~0x10;
  }

  if (cia->timer_a.control & 0x1) { /* Timer A Start */
    cia->timer_a.counter--;
    if (cia->timer_a.counter == 0) {
      if (cia->icr_mask & 0x1) { /* Timer A Underflow Interrupt */
        cia->icr_status |= 0x1;
        cia->icr_status |= 0x80;
        if (cia->no == 1) {
          mos6510_irq((mos6510_t *)cia->cpu, (mem_t *)cia->mem);
        } else if (cia->no == 2) {
          mos6510_nmi((mos6510_t *)cia->cpu, (mem_t *)cia->mem);
        }
      }

      cia->timer_a.counter = cia->timer_a.latch;
      if (cia->timer_a.control & 0x8) { /* Timer A One Shot */
        cia->timer_a.control &= ~0x1;
      }
    }
  }
  if (cia->timer_b.control & 0x1) { /* Timer B Start */
    cia->timer_b.counter--;
    if (cia->timer_b.counter == 0) {
      if (cia->icr_mask & 0x2) { /* Timer B Underflow Interrupt */
        cia->icr_status |= 0x2;
        cia->icr_status |= 0x80;
        if (cia->no == 1) {
          mos6510_irq((mos6510_t *)cia->cpu, (mem_t *)cia->mem);
        } else if (cia->no == 2) {
          mos6510_nmi((mos6510_t *)cia->cpu, (mem_t *)cia->mem);
        }
      }

      cia->timer_b.counter = cia->timer_b.latch;
      if (cia->timer_b.control & 0x8) { /* Timer B One Shot */
        cia->timer_b.control &= ~0x1;
      }
    }
  }
}



void cia_dump(FILE *fh, cia_t *cia)
{
  fprintf(fh, "CIA #%d\n", cia->no);
  fprintf(fh, "  ICR Status: 0x%02x\n", cia->icr_status);
  fprintf(fh, "  ICR Mask  : 0x%02x\n", cia->icr_mask);
  fprintf(fh, "  Timer A, Control: 0x%02x\n", cia->timer_a.control);
  fprintf(fh, "  Timer A, Latch  : 0x%04x\n", cia->timer_a.latch);
  fprintf(fh, "  Timer A, Counter: 0x%04x\n", cia->timer_a.counter);
  fprintf(fh, "  Timer B, Control: 0x%02x\n", cia->timer_b.control);
  fprintf(fh, "  Timer B, Latch  : 0x%04x\n", cia->timer_b.latch);
  fprintf(fh, "  Timer B, Counter: 0x%04x\n", cia->timer_b.counter);
}



