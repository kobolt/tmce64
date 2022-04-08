#ifndef _MOS6510_H
#define _MOS6510_H

#include <stdint.h>
#include <stdbool.h>
#include "mem.h"

typedef struct mos6510_status_s {
  uint8_t n : 1; /* Negative */
  uint8_t v : 1; /* Overflow */
  uint8_t b : 1; /* Break */
  uint8_t d : 1; /* Decimal */
  uint8_t i : 1; /* Interrupt Disable */
  uint8_t z : 1; /* Zero */
  uint8_t c : 1; /* Carry */
} mos6510_status_t;

typedef struct mos6510_s {
  uint16_t pc;         /* Program Counter */
  uint8_t a;           /* Accumulator */
  uint8_t x;           /* X Register */
  uint8_t y;           /* Y Register */
  uint8_t sp;          /* Stack Pointer */
  mos6510_status_t sr; /* Status Register */
} mos6510_t;

typedef bool (*mos6510_opcode_handler_t)(uint32_t, mos6510_t *, mem_t *);

#define MOS6510_VECTOR_NMI_LOW    0xFFFA
#define MOS6510_VECTOR_NMI_HIGH   0xFFFB
#define MOS6510_VECTOR_RESET_LOW  0xFFFC
#define MOS6510_VECTOR_RESET_HIGH 0xFFFD
#define MOS6510_VECTOR_IRQ_LOW    0xFFFE
#define MOS6510_VECTOR_IRQ_HIGH   0xFFFF

void mos6510_execute(mos6510_t *cpu, mem_t *mem);
void mos6510_reset(mos6510_t *cpu, mem_t *mem);
void mos6510_nmi(mos6510_t *cpu, mem_t *mem);
void mos6510_irq(mos6510_t *cpu, mem_t *mem);

void mos6510_trap_opcode(uint8_t opcode, mos6510_opcode_handler_t handler);

#endif /* _MOS6510_H */
