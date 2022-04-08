#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mos6510.h"
#include "mem.h"



#define MOS6510_TRACE_BUFFER_SIZE 20

typedef enum {
  AM_ACCU, /* A      - Accumulator */
  AM_IMPL, /* i      - Implied */
  AM_IMM,  /* #      - Immediate */
  AM_ABS,  /* a      - Absolute */
  AM_ABSI, /* (a)    - Indirect Absolute */
  AM_ABSX, /* a,x    - Absolute + X */
  AM_ABSY, /* a,y    - Absolute + Y */
  AM_REL,  /* r      - Relative */
  AM_ZP,   /* zp     - Zero Page */
  AM_ZPX,  /* zp,x   - Zero Page + X */
  AM_ZPY,  /* zp,y   - Zero Page + Y */
  AM_ZPYI, /* (zp),y - Zero Page Indirect Indexed */
  AM_ZPIX, /* (zp,x) - Zero Page Indexed Indirect */
  AM_NONE,
} mos6510_address_mode_t;

typedef struct mos6510_trace_s {
  mos6510_t cpu;
  uint8_t mc[3];
} mos6510_trace_t;



static mos6510_address_mode_t opcode_address_mode[UINT8_MAX + 1] = {
  AM_IMPL, AM_ZPIX, AM_NONE, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_NONE, AM_NONE, AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_NONE, AM_NONE, AM_NONE, AM_ABSX, AM_ABSX, AM_NONE,
  AM_ABS,  AM_ZPIX, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_NONE, AM_ABS,  AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_NONE, AM_NONE, AM_NONE, AM_ABSX, AM_ABSX, AM_NONE,
  AM_IMPL, AM_ZPIX, AM_NONE, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_NONE, AM_ABS,  AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_NONE, AM_NONE, AM_NONE, AM_ABSX, AM_ABSX, AM_NONE,
  AM_IMPL, AM_ZPIX, AM_NONE, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_ACCU, AM_NONE, AM_ABSI, AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_NONE, AM_NONE, AM_NONE, AM_ABSX, AM_ABSX, AM_NONE,
  AM_NONE, AM_ZPIX, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_NONE, AM_IMPL, AM_NONE, AM_ABS,  AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_ZPY,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_NONE, AM_NONE, AM_ABSX, AM_NONE, AM_NONE,
  AM_IMM,  AM_ZPIX, AM_IMM,  AM_NONE, AM_ZP,   AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_NONE, AM_ABS,  AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_ZPY,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_IMPL, AM_NONE, AM_ABSX, AM_ABSX, AM_ABSY, AM_NONE,
  AM_IMM,  AM_ZPIX, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_NONE, AM_ABS,  AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_NONE, AM_NONE, AM_NONE, AM_ABSX, AM_ABSX, AM_NONE,
  AM_IMM,  AM_ZPIX, AM_NONE, AM_NONE, AM_ZP,   AM_ZP,   AM_ZP,   AM_NONE,
  AM_IMPL, AM_IMM,  AM_IMPL, AM_NONE, AM_ABS,  AM_ABS,  AM_ABS,  AM_NONE,
  AM_REL,  AM_ZPYI, AM_NONE, AM_NONE, AM_NONE, AM_ZPX,  AM_ZPX,  AM_NONE,
  AM_IMPL, AM_ABSY, AM_NONE, AM_NONE, AM_NONE, AM_ABSX, AM_ABSX, AM_NONE,
};



static char *opcode_mnemonic[UINT8_MAX + 1] = {
  "BRK", "ORA", "---", "---", "---", "ORA", "ASL", "---", 
  "PHP", "ORA", "ASL", "---", "---", "ORA", "ASL", "---",
  "BPL", "ORA", "---", "---", "---", "ORA", "ASL", "---",
  "CLC", "ORA", "---", "---", "---", "ORA", "ASL", "---",
  "JSR", "AND", "---", "---", "BIT", "AND", "ROL", "---",
  "PLP", "AND", "ROL", "---", "BIT", "AND", "ROL", "---",
  "BMI", "AND", "---", "---", "---", "AND", "ROL", "---",
  "SEC", "AND", "---", "---", "---", "AND", "ROL", "---",
  "RTI", "EOR", "---", "---", "---", "EOR", "LSR", "---",
  "PHA", "EOR", "LSR", "---", "JMP", "EOR", "LSR", "---",
  "BVC", "EOR", "---", "---", "---", "EOR", "LSR", "---",
  "CLI", "EOR", "---", "---", "---", "EOR", "LSR", "---",
  "RTS", "ADC", "---", "---", "---", "ADC", "ROR", "---",
  "PLA", "ADC", "ROR", "---", "JMP", "ADC", "ROR", "---",
  "BVS", "ADC", "---", "---", "---", "ADC", "ROR", "---",
  "SEI", "ADC", "---", "---", "---", "ADC", "ROR", "---",
  "---", "STA", "---", "---", "STY", "STA", "STX", "---",
  "DEY", "---", "TXA", "---", "STY", "STA", "STX", "---",
  "BCC", "STA", "---", "---", "STY", "STA", "STX", "---",
  "TYA", "STA", "TXS", "---", "---", "STA", "---", "---",
  "LDY", "LDA", "LDX", "---", "LDY", "LDA", "LDX", "---",
  "TAY", "LDA", "TAX", "---", "LDY", "LDA", "LDX", "---",
  "BCS", "LDA", "---", "---", "LDY", "LDA", "LDX", "---",
  "CLV", "LDA", "TSX", "---", "LDY", "LDA", "LDX", "---",
  "CPY", "CMP", "---", "---", "CPY", "CMP", "DEC", "---",
  "INY", "CMP", "DEX", "---", "CPY", "CMP", "DEC", "---",
  "BNE", "CMP", "---", "---", "---", "CMP", "DEC", "---",
  "CLD", "CMP", "---", "---", "---", "CMP", "DEC", "---",
  "CPX", "SBC", "---", "---", "CPX", "SBC", "INC", "---",
  "INX", "SBC", "NOP", "---", "CPX", "SBC", "INC", "---",
  "BEQ", "SBC", "---", "---", "---", "SBC", "INC", "---",
  "SED", "SBC", "---", "---", "---", "SBC", "INC", "---",
};



static mos6510_trace_t mos6510_trace_buffer[MOS6510_TRACE_BUFFER_SIZE];
static int mos6510_trace_index = 0;



static void mos6510_disassemble(FILE *fh, uint16_t pc, uint8_t mc[3])
{
  uint16_t address;
  int8_t relative;

  switch (opcode_address_mode[mc[0]]) {
  case AM_ACCU:
  case AM_IMPL:
    fprintf(fh, "%02X          ", mc[0]);
    break;

  case AM_IMM:
  case AM_REL:
  case AM_ZP:
  case AM_ZPX:
  case AM_ZPY:
  case AM_ZPYI:
  case AM_ZPIX:
    fprintf(fh, "%02X %02X       ", mc[0], mc[1]);
    break;

  case AM_ABS:
  case AM_ABSI:
  case AM_ABSX:
  case AM_ABSY:
    fprintf(fh, "%02X %02X %02X    ", mc[0], mc[1], mc[2]);
    break;

  case AM_NONE:
  default:
    fprintf(fh, "-           ");
    break;
  }

  fprintf(fh, "%s ", opcode_mnemonic[mc[0]]);

  switch (opcode_address_mode[mc[0]]) {
  case AM_ACCU:
    fprintf(fh, "A       ");
    break;

  case AM_IMPL:
    fprintf(fh, "        ");
    break;

  case AM_IMM:
    fprintf(fh, "#$%02X    ", mc[1]);
    break;

  case AM_ABS:
    fprintf(fh, "$%02X%02X   ", mc[2], mc[1]);
    break;

  case AM_ABSI:
    fprintf(fh, "($%02X%02X) ", mc[2], mc[1]);
    break;

  case AM_ABSX:
    fprintf(fh, "$%02X%02X,X ", mc[2], mc[1]);
    break;

  case AM_ABSY:
    fprintf(fh, "$%02X%02X,Y ", mc[2], mc[1]);
    break;

  case AM_REL:
    address = pc + 2;
    relative = mc[1];
    address += relative;
    fprintf(fh, "$%04X   ", address);
    break;

  case AM_ZP:
    fprintf(fh, "$%02X     ", mc[1]);
    break;

  case AM_ZPX:
    fprintf(fh, "$%02X,X   ", mc[1]);
    break;

  case AM_ZPY:
    fprintf(fh, "$%02X,Y   ", mc[1]);
    break;

  case AM_ZPYI:
    fprintf(fh, "($%02X),Y ", mc[1]);
    break;

  case AM_ZPIX:
    fprintf(fh, "($%02X,X) ", mc[1]);
    break;

  case AM_NONE:
  default:
    fprintf(fh, "-       ");
    break;
  }
}



static void mos6510_register_dump(FILE *fh, mos6510_t *cpu, uint8_t mc[3])
{
  fprintf(fh, ".C:%04x  ", cpu->pc);
  mos6510_disassemble(fh, cpu->pc, mc);
  fprintf(fh, "   - ");
  fprintf(fh, "A:%02X ", cpu->a);
  fprintf(fh, "X:%02X ", cpu->x);
  fprintf(fh, "Y:%02X ", cpu->y);
  fprintf(fh, "SP:%02x ", cpu->sp);
  fprintf(fh, "%c", (cpu->sr.n) ? 'N' : '.');
  fprintf(fh, "%c", (cpu->sr.v) ? 'V' : '.');
  fprintf(fh, "-");
  fprintf(fh, "%c", (cpu->sr.b) ? 'B' : '.');
  fprintf(fh, "%c", (cpu->sr.d) ? 'D' : '.');
  fprintf(fh, "%c", (cpu->sr.i) ? 'I' : '.');
  fprintf(fh, "%c", (cpu->sr.z) ? 'Z' : '.');
  fprintf(fh, "%c", (cpu->sr.c) ? 'C' : '.');
  fprintf(fh, "\n");
}



void mos6510_trace_init(void)
{
  memset(mos6510_trace_buffer, 0,
    MOS6510_TRACE_BUFFER_SIZE * sizeof(mos6510_trace_t));
}



void mos6510_trace_dump(FILE *fh)
{
  int i;

  for (i = 0; i < MOS6510_TRACE_BUFFER_SIZE; i++) {
    mos6510_trace_index++;
    if (mos6510_trace_index >= MOS6510_TRACE_BUFFER_SIZE) {
      mos6510_trace_index = 0;
    }
    mos6510_register_dump(fh, &mos6510_trace_buffer[mos6510_trace_index].cpu,
                               mos6510_trace_buffer[mos6510_trace_index].mc);
  }
}



void mos6510_trace_add(mos6510_t *cpu, mem_t *mem)
{
  uint8_t mc[3];

  mos6510_trace_index++;
  if (mos6510_trace_index >= MOS6510_TRACE_BUFFER_SIZE) {
    mos6510_trace_index = 0;
  }

  memcpy(&mos6510_trace_buffer[mos6510_trace_index].cpu,
    cpu, sizeof(mos6510_t));
  mc[0] = mem_read(mem, cpu->pc);
  mc[1] = mem_read(mem, cpu->pc + 1);
  mc[2] = mem_read(mem, cpu->pc + 2);
  memcpy(&mos6510_trace_buffer[mos6510_trace_index].mc,
    mc, sizeof(uint8_t) * 3);
}



