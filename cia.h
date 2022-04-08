#ifndef _CIA_H
#define _CIA_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct cia_timer_s {
  uint8_t control;
  uint16_t latch;
  uint16_t counter;
} cia_timer_t;

typedef struct cia_s {
  int no;
  uint8_t icr_status;
  uint8_t icr_mask;
  cia_timer_t timer_a;
  cia_timer_t timer_b;
  void *cpu;
  void *mem;
} cia_t;

#define CIA_PRA       0x0 /* Data Port A */
#define CIA_PRB       0x1 /* Data Port B */
#define CIA_DDRA      0x2 /* Data Direction Port A */
#define CIA_DDRB      0x3 /* Data Direction Port B */
#define CIA_TA_LO     0x4 /* Timer A Low Byte */
#define CIA_TA_HI     0x5 /* Timer A High Byte */
#define CIA_TB_LO     0x6 /* Timer B Low Byte */
#define CIA_TB_HI     0x7 /* Timer B High Byte */
#define CIA_TOD_10THS 0x8 /* Real Time Clock 1/10s */
#define CIA_TOD_SEC   0x9 /* Real Time Clock Seconds */
#define CIA_TOD_MIN   0xA /* Real Time Clock Minutes */
#define CIA_TOD_HR    0xB /* Real Time Clock Hour */
#define CIA_SDR       0xC /* Serial Shift Register */
#define CIA_ICR       0xD /* Interrupt Control and Status */
#define CIA_CRA       0xE /* Control Timer A */
#define CIA_CRB       0xF /* Control Timer B */

uint8_t cia_read_hook(void *cia, uint16_t address);
void cia_write_hook(void *cia, uint16_t address, uint8_t value);
void cia_init(cia_t *cia, int cia_no);
void cia_execute(cia_t *cia);
void cia_dump(FILE *fh, cia_t *cia);

#endif /* _CIA_H */
