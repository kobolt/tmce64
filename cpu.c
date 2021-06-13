#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cpu.h"
#include "mem.h"
#include "panic.h"



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
} cpu_address_mode_t;

typedef enum {
  OP_ADC, /* Add Memory to Accumulator with Carry */
  OP_AND, /* AND Memory with Accumulator */
  OP_ASL, /* Arithmetic Shift Left One Bit */
  OP_BCC, /* Branch on Carry Clear */
  OP_BCS, /* Branch on Carry Set */
  OP_BEQ, /* Branch on Result Zero */
  OP_BIT, /* Test Bits in Memory with Accumulator */
  OP_BMI, /* Branch on Result Minus */
  OP_BNE, /* Branch on Result not Zero */
  OP_BPL, /* Branch on Result Plus */
  OP_BRK, /* Break */
  OP_BVC, /* Branch on Overflow Clear */
  OP_BVS, /* Branch on Overflow Set */
  OP_CLC, /* Clear Carry Flag */
  OP_CLD, /* Clear Decimal Mode */
  OP_CLI, /* Clear Interrupt Disable Status */
  OP_CLV, /* Clear Overflow Flag */
  OP_CMP, /* Compare Memory and Accumulator */
  OP_CPX, /* Compare Memory and Index X */
  OP_CPY, /* Compare Memory with Index Y */
  OP_DEC, /* Decrement Memory by One */
  OP_DEX, /* Decrement Index X by One */
  OP_DEY, /* Decrement Index Y by One */
  OP_EOR, /* XOR Memory with Accumulator */
  OP_INC, /* Increment Memory by One */
  OP_INX, /* Increment Index X by One */
  OP_INY, /* Increment Index Y by One */
  OP_JMP, /* Jump to New Location */
  OP_JSR, /* Jump to New Location Saving Return Address */
  OP_LDA, /* Load Accumulator with Memory */
  OP_LDX, /* Load Index X with Memory */
  OP_LDY, /* Load Index Y with Memory */
  OP_LSR, /* Logical Shift Right One Bit */
  OP_NOP, /* No Operation */
  OP_ORA, /* OR Memory with Accumulator */
  OP_PHA, /* Push Accumulator on Stack */
  OP_PHP, /* Push Processor Status on Stack */
  OP_PLA, /* Pull Accumulator from Stack */
  OP_PLP, /* Pull Processor Status from Stack */
  OP_ROL, /* Rotate Left One Bit */
  OP_ROR, /* Rotate Right One Bit */
  OP_RTI, /* Return from Interrupt */
  OP_RTS, /* Return from Subroutine */
  OP_SBC, /* Subtract Memory from Accumulator with Borrow */
  OP_SEC, /* Set Carry Flag */
  OP_SED, /* Set Decimal Mode */
  OP_SEI, /* Set Interrupt Disable Status */
  OP_STA, /* Store Accumulator in Memory */
  OP_STX, /* Store Index X in Memory */
  OP_STY, /* Store Index Y in Memory */
  OP_TAX, /* Transfer Accumulator to Index X */
  OP_TAY, /* Transfer Accumulator to Index Y */
  OP_TSX, /* Transfer Stack Pointer to Index X */
  OP_TXA, /* Transfer Index X to Accumulator */
  OP_TXS, /* Transfer Index X to Stack Pointer */
  OP_TYA, /* Transfer Index Y to Accumulator */
  OP_NONE,
} cpu_operation_t;

typedef struct cpu_trace_s {
  cpu_t cpu;
  uint8_t mc[3];
} cpu_trace_t;

#define CPU_TRACE_BUFFER_SIZE 20



static cpu_address_mode_t opcode_address_mode[UINT8_MAX + 1] = {
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

static cpu_operation_t opcode_operation[UINT8_MAX + 1] = {
  OP_BRK,  OP_ORA,  OP_NONE, OP_NONE, OP_NONE, OP_ORA,  OP_ASL,  OP_NONE, 
  OP_PHP,  OP_ORA,  OP_ASL,  OP_NONE, OP_NONE, OP_ORA,  OP_ASL,  OP_NONE,
  OP_BPL,  OP_ORA,  OP_NONE, OP_NONE, OP_NONE, OP_ORA,  OP_ASL,  OP_NONE,
  OP_CLC,  OP_ORA,  OP_NONE, OP_NONE, OP_NONE, OP_ORA,  OP_ASL,  OP_NONE,
  OP_JSR,  OP_AND,  OP_NONE, OP_NONE, OP_BIT,  OP_AND,  OP_ROL,  OP_NONE,
  OP_PLP,  OP_AND,  OP_ROL,  OP_NONE, OP_BIT,  OP_AND,  OP_ROL,  OP_NONE,
  OP_BMI,  OP_AND,  OP_NONE, OP_NONE, OP_NONE, OP_AND,  OP_ROL,  OP_NONE,
  OP_SEC,  OP_AND,  OP_NONE, OP_NONE, OP_NONE, OP_AND,  OP_ROL,  OP_NONE,
  OP_RTI,  OP_EOR,  OP_NONE, OP_NONE, OP_NONE, OP_EOR,  OP_LSR,  OP_NONE,
  OP_PHA,  OP_EOR,  OP_LSR,  OP_NONE, OP_JMP,  OP_EOR,  OP_LSR,  OP_NONE,
  OP_BVC,  OP_EOR,  OP_NONE, OP_NONE, OP_NONE, OP_EOR,  OP_LSR,  OP_NONE,
  OP_CLI,  OP_EOR,  OP_NONE, OP_NONE, OP_NONE, OP_EOR,  OP_LSR,  OP_NONE,
  OP_RTS,  OP_ADC,  OP_NONE, OP_NONE, OP_NONE, OP_ADC,  OP_ROR,  OP_NONE,
  OP_PLA,  OP_ADC,  OP_ROR,  OP_NONE, OP_JMP,  OP_ADC,  OP_ROR,  OP_NONE,
  OP_BVS,  OP_ADC,  OP_NONE, OP_NONE, OP_NONE, OP_ADC,  OP_ROR,  OP_NONE,
  OP_SEI,  OP_ADC,  OP_NONE, OP_NONE, OP_NONE, OP_ADC,  OP_ROR,  OP_NONE,
  OP_NONE, OP_STA,  OP_NONE, OP_NONE, OP_STY,  OP_STA,  OP_STX,  OP_NONE,
  OP_DEY,  OP_NONE, OP_TXA,  OP_NONE, OP_STY,  OP_STA,  OP_STX,  OP_NONE,
  OP_BCC,  OP_STA,  OP_NONE, OP_NONE, OP_STY,  OP_STA,  OP_STX,  OP_NONE,
  OP_TYA,  OP_STA,  OP_TXS,  OP_NONE, OP_NONE, OP_STA,  OP_NONE, OP_NONE,
  OP_LDY,  OP_LDA,  OP_LDX,  OP_NONE, OP_LDY,  OP_LDA,  OP_LDX,  OP_NONE,
  OP_TAY,  OP_LDA,  OP_TAX,  OP_NONE, OP_LDY,  OP_LDA,  OP_LDX,  OP_NONE,
  OP_BCS,  OP_LDA,  OP_NONE, OP_NONE, OP_LDY,  OP_LDA,  OP_LDX,  OP_NONE,
  OP_CLV,  OP_LDA,  OP_TSX,  OP_NONE, OP_LDY,  OP_LDA,  OP_LDX,  OP_NONE,
  OP_CPY,  OP_CMP,  OP_NONE, OP_NONE, OP_CPY,  OP_CMP,  OP_DEC,  OP_NONE,
  OP_INY,  OP_CMP,  OP_DEX,  OP_NONE, OP_CPY,  OP_CMP,  OP_DEC,  OP_NONE,
  OP_BNE,  OP_CMP,  OP_NONE, OP_NONE, OP_NONE, OP_CMP,  OP_DEC,  OP_NONE,
  OP_CLD,  OP_CMP,  OP_NONE, OP_NONE, OP_NONE, OP_CMP,  OP_DEC,  OP_NONE,
  OP_CPX,  OP_SBC,  OP_NONE, OP_NONE, OP_CPX,  OP_SBC,  OP_INC,  OP_NONE,
  OP_INX,  OP_SBC,  OP_NOP,  OP_NONE, OP_CPX,  OP_SBC,  OP_INC,  OP_NONE,
  OP_BEQ,  OP_SBC,  OP_NONE, OP_NONE, OP_NONE, OP_SBC,  OP_INC,  OP_NONE,
  OP_SED,  OP_SBC,  OP_NONE, OP_NONE, OP_NONE, OP_SBC,  OP_INC,  OP_NONE,
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

static cpu_opcode_handler_t cpu_trap_opcode_handler = NULL;

static cpu_trace_t cpu_trace_buffer[CPU_TRACE_BUFFER_SIZE];
static int cpu_trace_index = 0;



static void cpu_dump_disassemble(FILE *fh, uint16_t pc, uint8_t mc[3])
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
    fprintf(fh, "-       ");
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



void cpu_trap_opcode(uint8_t opcode, cpu_opcode_handler_t handler)
{
  opcode_mnemonic[opcode] = "TRP";
  opcode_address_mode[opcode] = AM_IMPL;
  cpu_trap_opcode_handler = handler;
}



static void cpu_register_dump(FILE *fh, cpu_t *cpu, uint8_t mc[3])
{
  fprintf(fh, ".C:%04x  ", cpu->pc);
  cpu_dump_disassemble(fh, cpu->pc, mc);
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



void cpu_trace_init(void)
{
  memset(cpu_trace_buffer, 0, CPU_TRACE_BUFFER_SIZE * sizeof(cpu_trace_t));
}



void cpu_trace_dump(FILE *fh)
{
  int i;

  for (i = 0; i < CPU_TRACE_BUFFER_SIZE; i++) {
    cpu_trace_index++;
    if (cpu_trace_index >= CPU_TRACE_BUFFER_SIZE) {
      cpu_trace_index = 0;
    }
    cpu_register_dump(fh, &cpu_trace_buffer[cpu_trace_index].cpu,
                           cpu_trace_buffer[cpu_trace_index].mc);
  }
}



void cpu_trace_add(cpu_t *cpu, mem_t *mem)
{
  uint8_t mc[3];

  if (cpu_trace_buffer[cpu_trace_index].cpu.pc == cpu->pc) {
    panic("Panic! Stuck program counter!\n");
  }

  cpu_trace_index++;
  if (cpu_trace_index >= CPU_TRACE_BUFFER_SIZE) {
    cpu_trace_index = 0;
  }

  memcpy(&cpu_trace_buffer[cpu_trace_index].cpu, cpu, sizeof(cpu_t));
  mc[0] = mem_read(mem, cpu->pc);
  mc[1] = mem_read(mem, cpu->pc + 1);
  mc[2] = mem_read(mem, cpu->pc + 2);
  memcpy(&cpu_trace_buffer[cpu_trace_index].mc, mc, sizeof(uint8_t) * 3);
}



void cpu_reset(cpu_t *cpu, mem_t *mem)
{
  cpu->pc  = mem_read(mem, MEM_VECTOR_RESET_LOW);
  cpu->pc += mem_read(mem, MEM_VECTOR_RESET_HIGH) * 256;
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->sp = 0;
  cpu->sr.n = 0;
  cpu->sr.v = 0;
  cpu->sr.b = 0;
  cpu->sr.d = 0;
  cpu->sr.i = 0;
  cpu->sr.z = 0;
  cpu->sr.c = 0;
}



static uint8_t cpu_status_get(cpu_t *cpu)
{
  /* NOTE: Break flag is set, as needed by PHP and BRK. */
  return ((cpu->sr.n << 7) +
          (cpu->sr.v << 6) +
          (1         << 5) +
          (1         << 4) +
          (cpu->sr.d << 3) +
          (cpu->sr.i << 2) +
          (cpu->sr.z << 1) +
           cpu->sr.c);
}



static void cpu_status_set(cpu_t *cpu, uint8_t flags)
{
  /* NOTE: Break flag is discarded, as needed by PLP and RTI. */
  cpu->sr.n = (flags >> 7) & 0x1;
  cpu->sr.v = (flags >> 6) & 0x1;
  cpu->sr.d = (flags >> 3) & 0x1;
  cpu->sr.i = (flags >> 2) & 0x1;
  cpu->sr.z = (flags >> 1) & 0x1;
  cpu->sr.c =  flags       & 0x1;
}



static inline uint8_t dec_to_bin(uint8_t value)
{
  return (value % 0x10) + ((value / 0x10) * 10);
}

static inline uint8_t bin_to_dec(uint8_t value)
{
  return (value % 10) + ((value / 10) * 0x10);
}



static inline void cpu_flag_zero_other(cpu_t *cpu, uint8_t value)
{
  if (value == 0) {
    cpu->sr.z = 1;
  } else {
    cpu->sr.z = 0;
  }
}

static inline void cpu_flag_negative_other(cpu_t *cpu, uint8_t value)
{
  if ((value >> 7) == 1) {
    cpu->sr.n = 1;
  } else {
    cpu->sr.n = 0;
  }
}

static inline void cpu_flag_zero_compare(cpu_t *cpu, uint8_t a, uint8_t b)
{
  if (a == b) {
    cpu->sr.z = 1;
  } else {
    cpu->sr.z = 0;
  }
}

static inline void cpu_flag_negative_compare(cpu_t *cpu, uint8_t a, uint8_t b)
{
  if (a < b) {
    cpu->sr.n = 1;
  } else {
    cpu->sr.n = 0;
  }
}

static inline void cpu_flag_carry_compare(cpu_t *cpu, uint8_t a, uint8_t b)
{
  if (a >= b) {
    cpu->sr.c = 1;
  } else {
    cpu->sr.c = 0;
  }
}

static inline bool cpu_flag_carry_add(cpu_t *cpu, uint8_t value)
{
  if (cpu->sr.d == 1) {
    if (cpu->a + value + cpu->sr.c > 99) {
      return 1;
    } else {
      return 0;
    }
  } else {
    if (cpu->a + value + cpu->sr.c > 0xFF) {
      return 1;
    } else {
      return 0;
    }
  }
}

static inline bool cpu_flag_carry_sub(cpu_t *cpu, uint8_t value)
{
  bool borrow;
  if (cpu->sr.c == 0) {
    borrow = 1;
  } else {
    borrow = 0;
  }
  if (cpu->a - value - borrow < 0) {
    return 0;
  } else {
    return 1;
  }
}

static inline void cpu_flag_overflow_add(cpu_t *cpu, uint8_t a, uint8_t b)
{
  if (a >> 7 == b >> 7) {
    if (cpu->a >> 7 != a >> 7) {
      cpu->sr.v = 1;
    } else {
      cpu->sr.v = 0;
    }
  } else {
    cpu->sr.v = 0;
  }
}

static inline void cpu_flag_overflow_sub(cpu_t *cpu, uint8_t a, uint8_t b)
{
  if (a >> 7 != b >> 7) {
    if (cpu->a >> 7 != a >> 7) {
      cpu->sr.v = 1;
    } else {
      cpu->sr.v = 0;
    }
  } else {
    cpu->sr.v = 0;
  }
}

static inline void cpu_flag_overflow_bit(cpu_t *cpu, uint8_t value)
{
  if (((value >> 6) & 0x1) == 1) {
    cpu->sr.v = 1;
  } else {
    cpu->sr.v = 0;
  }
}



void cpu_execute(cpu_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  int8_t relative;
  uint8_t zeropage;
  uint8_t initial;
  uint8_t value;
  uint16_t absolute;
  uint16_t address;
  bool bit;
  cpu_address_mode_t am;
  cpu_operation_t op;

  opcode = mem_read(mem, cpu->pc++);
  am = opcode_address_mode[opcode];
  op = opcode_operation[opcode];

  switch (am) {
  case AM_ACCU: /* Accumulator */
    value = cpu->a;
    break;

  case AM_IMM: /* Immediate */
    value = mem_read(mem, cpu->pc++);
    break;

  case AM_ABS: /* Absolute */
    absolute  = mem_read(mem, cpu->pc++);
    absolute += mem_read(mem, cpu->pc++) * 256;
    value   = mem_read(mem, absolute);
    address = absolute;
    break;

  case AM_ABSI: /* Indirect Absolute */
    absolute  = mem_read(mem, cpu->pc++);
    absolute += mem_read(mem, cpu->pc++) * 256;
    address = mem_read(mem, absolute);
    absolute += 1;
    address += mem_read(mem, absolute) * 256;
    break;

  case AM_ABSX: /* Absolute + X */
    absolute  = mem_read(mem, cpu->pc++);
    absolute += mem_read(mem, cpu->pc++) * 256;
    absolute += cpu->x;
    value   = mem_read(mem, absolute);
    address = absolute;
    break;

  case AM_ABSY: /* Absolute + Y */
    absolute  = mem_read(mem, cpu->pc++);
    absolute += mem_read(mem, cpu->pc++) * 256;
    absolute += cpu->y;
    value   = mem_read(mem, absolute);
    address = absolute;
    break;

  case AM_REL: /* Relative */
    relative = mem_read(mem, cpu->pc++);
    break;

  case AM_ZP: /* Zero Page */
    zeropage = mem_read(mem, cpu->pc++);
    value   = mem_read(mem, zeropage);
    address = zeropage;
    break;

  case AM_ZPX: /* Zero Page + X */
    zeropage = mem_read(mem, cpu->pc++);
    zeropage += cpu->x;
    value   = mem_read(mem, zeropage);
    address = zeropage;
    break;

  case AM_ZPY: /* Zero Page + Y */
    zeropage = mem_read(mem, cpu->pc++);
    zeropage += cpu->y;
    value   = mem_read(mem, zeropage);
    address = zeropage;
    break;

  case AM_ZPYI: /* Zero Page Indirect Indexed */
    zeropage = mem_read(mem, cpu->pc++);
    absolute  = mem_read(mem, zeropage);
    zeropage += 1;
    absolute += mem_read(mem, zeropage) * 256;
    absolute += cpu->y;
    value   = mem_read(mem, absolute);
    address = absolute;
    break;

  case AM_ZPIX: /* Zero Page Indexed Indirect */
    zeropage = mem_read(mem, cpu->pc++);
    zeropage += cpu->x;
    absolute  = mem_read(mem, zeropage);
    zeropage += 1;
    absolute += mem_read(mem, zeropage) * 256;
    value   = mem_read(mem, absolute);
    address = absolute;
    break;

  case AM_IMPL:
  case AM_NONE:
  default:
    break;
  }

  switch (op) {
  case OP_ADC: /* Add Memory to Accumulator with Carry */
    if (cpu->sr.d == 1) {
      value = dec_to_bin(value);
      cpu->a = dec_to_bin(cpu->a);
    }
    initial = cpu->a;
    bit = cpu_flag_carry_add(cpu, value);
    cpu->a += value;
    cpu->a += cpu->sr.c;
    if (cpu->sr.d == 1) {
      if (cpu->a > 99) {
        cpu->a %= 100;
      }
      cpu->a = bin_to_dec(cpu->a);
    }
    cpu->sr.c = bit;
    cpu_flag_overflow_add(cpu, initial, value);
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_AND: /* AND Memory with Accumulator */
    cpu->a &= value;
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_ASL: /* Arithmetic Shift Left One Bit */
    bit = value & 0b10000000;
    value = value << 1;
    if (am == AM_ACCU) {
      cpu->a = value;
    } else {
      mem_write(mem, address, value);
    }
    cpu->sr.c = bit;
    cpu_flag_negative_other(cpu, value);
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_BCC: /* Branch on Carry Clear */
    if (cpu->sr.c == 0) {
      cpu->pc += relative;
    }
    break;

  case OP_BCS: /* Branch on Carry Set */
    if (cpu->sr.c == 1) {
      cpu->pc += relative;
    }
    break;

  case OP_BEQ: /* Branch on Result Zero */
    if (cpu->sr.z == 1) {
      cpu->pc += relative;
    }
    break;

  case OP_BIT: /* Test Bits in Memory with Accumulator */
    cpu_flag_overflow_bit(cpu, value);
    cpu_flag_negative_other(cpu, value);
    value &= cpu->a;
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_BMI: /* Branch on Result Minus */
    if (cpu->sr.n == 1) {
      cpu->pc += relative;
    }
    break;

  case OP_BNE: /* Branch on Result not Zero */
    if (cpu->sr.z == 0) {
      cpu->pc += relative;
    }
    break;

  case OP_BPL: /* Branch on Result Plus */
    if (cpu->sr.n == 0) {
      cpu->pc += relative;
    }
    break;

  case OP_BRK: /* Break */
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc + 1) / 256);
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc + 1) % 256);
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu_status_get(cpu));
    cpu->sr.i = 1;
    cpu->sr.b = 1; /* Set for visualization only, not really used. */
    cpu->pc  = mem_read(mem, MEM_VECTOR_IRQ_LOW);
    cpu->pc += mem_read(mem, MEM_VECTOR_IRQ_HIGH) * 256;
    break;

  case OP_BVC: /* Branch on Overflow Clear */
    if (cpu->sr.v == 0) {
      cpu->pc += relative;
    }
    break;

  case OP_BVS: /* Branch on Overflow Set */
    if (cpu->sr.v == 1) {
      cpu->pc += relative;
    }
    break;

  case OP_CLC: /* Clear Carry Flag */
    cpu->sr.c = 0;
    break;

  case OP_CLD: /* Clear Decimal Mode */
    cpu->sr.d = 0;
    break;

  case OP_CLI: /* Clear Interrupt Disable Status */
    cpu->sr.i = 0;
    break;

  case OP_CLV: /* Clear Overflow Flag */
    cpu->sr.v = 0;
    break;

  case OP_CMP: /* Compare Memory and Accumulator */
    cpu_flag_negative_compare(cpu, cpu->a, value);
    cpu_flag_zero_compare(cpu, cpu->a, value);
    cpu_flag_carry_compare(cpu, cpu->a, value);
    break;

  case OP_CPX: /* Compare Memory and Index X */
    cpu_flag_negative_compare(cpu, cpu->x, value);
    cpu_flag_zero_compare(cpu, cpu->x, value);
    cpu_flag_carry_compare(cpu, cpu->x, value);
    break;

  case OP_CPY: /* Compare Memory with Index Y */
    cpu_flag_negative_compare(cpu, cpu->y, value);
    cpu_flag_zero_compare(cpu, cpu->y, value);
    cpu_flag_carry_compare(cpu, cpu->y, value);
    break;

  case OP_DEC: /* Decrement Memory by One */
    value -= 1;
    mem_write(mem, address, value);
    cpu_flag_negative_other(cpu, value);
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_DEX: /* Decrement Index X by One */
    cpu->x--;
    cpu_flag_negative_other(cpu, cpu->x);
    cpu_flag_zero_other(cpu, cpu->x);
    break;

  case OP_DEY: /* Decrement Index Y by One */
    cpu->y--;
    cpu_flag_negative_other(cpu, cpu->y);
    cpu_flag_zero_other(cpu, cpu->y);
    break;

  case OP_EOR: /* XOR Memory with Accumulator */
    cpu->a ^= value;
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_INC: /* Increment Memory by One */
    value += 1;
    mem_write(mem, address, value);
    cpu_flag_negative_other(cpu, value);
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_INX: /* Increment Index X by One */
    cpu->x++;
    cpu_flag_negative_other(cpu, cpu->x);
    cpu_flag_zero_other(cpu, cpu->x);
    break;

  case OP_INY: /* Increment Index Y by One */
    cpu->y++;
    cpu_flag_negative_other(cpu, cpu->y);
    cpu_flag_zero_other(cpu, cpu->y);
    break;

  case OP_JMP: /* Jump to New Location */
    cpu->pc = address;
    break;

  case OP_JSR: /* Jump to New Location Saving Return Address */
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc - 1) / 256);
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc - 1) % 256);
    cpu->pc = address;
    break;

  case OP_LDA: /* Load Accumulator with Memory */
    if (am == AM_IMM) {
      cpu->a = value;
    } else {
      cpu->a = mem_read(mem, address);
    }
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_LDX: /* Load Index X with Memory */
    if (am == AM_IMM) {
      cpu->x = value;
    } else {
      cpu->x = mem_read(mem, address);
    }
    cpu_flag_negative_other(cpu, cpu->x);
    cpu_flag_zero_other(cpu, cpu->x);
    break;

  case OP_LDY: /* Load Index Y with Memory */
    if (am == AM_IMM) {
      cpu->y = value;
    } else {
      cpu->y = mem_read(mem, address);
    }
    cpu_flag_negative_other(cpu, cpu->y);
    cpu_flag_zero_other(cpu, cpu->y);
    break;

  case OP_LSR: /* Logical Shift Right One Bit */
    bit = value & 0b00000001;
    value = value >> 1;
    if (am == AM_ACCU) {
      cpu->a = value;
    } else {
      mem_write(mem, address, value);
    }
    cpu->sr.c = bit;
    cpu_flag_negative_other(cpu, value);
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_NOP: /* No Operation */
    break;

  case OP_ORA: /* OR Memory with Accumulator */
    cpu->a |= value;
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_PHA: /* Push Accumulator on Stack */
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu->a);
    break;

  case OP_PHP: /* Push Processor Status on Stack */
    mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu_status_get(cpu));
    break;

  case OP_PLA: /* Pull Accumulator from Stack */
    cpu->a = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_PLP: /* Pull Processor Status from Stack */
    cpu_status_set(cpu, mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)));
    break;

  case OP_ROL: /* Rotate Left One Bit */
    bit = value & 0b10000000;
    value = value << 1;
    if (cpu->sr.c == 1) {
      value |= 0b00000001;
    }
    if (am == AM_ACCU) {
      cpu->a = value;
    } else {
      mem_write(mem, address, value);
    }
    cpu->sr.c = bit;
    cpu_flag_negative_other(cpu, value);
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_ROR: /* Rotate Right One Bit */
    bit = value & 0b00000001;
    value = value >> 1;
    if (cpu->sr.c == 1) {
      value |= 0b10000000;
    }
    if (am == AM_ACCU) {
      cpu->a = value;
    } else {
      mem_write(mem, address, value);
    }
    cpu->sr.c = bit;
    cpu_flag_negative_other(cpu, value);
    cpu_flag_zero_other(cpu, value);
    break;

  case OP_RTI: /* Return from Interrupt */
    cpu->sr.b = 0; /* Cleared for visualization only, not really used. */
    cpu_status_set(cpu, mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)));
    cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
    cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)) * 256;
    break;

  case OP_RTS: /* Return from Subroutine */
    cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
    cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)) * 256;
    cpu->pc += 1;
    break;

  case OP_SBC: /* Subtract Memory from Accumulator with Borrow */
    if (cpu->sr.d == 1) {
      value = dec_to_bin(value);
      cpu->a = dec_to_bin(cpu->a);
    }
    initial = cpu->a;
    bit = cpu_flag_carry_sub(cpu, value);
    cpu->a -= value;
    if (cpu->sr.c == 0) {
      cpu->a -= 1;
    }
    if (cpu->sr.d == 1) {
      if (cpu->a > 99) {
        cpu->a -= (255 - 99);
      }
      cpu->a = bin_to_dec(cpu->a);
    }
    cpu->sr.c = bit;
    cpu_flag_overflow_sub(cpu, initial, value);
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_SEC: /* Set Carry Flag */
    cpu->sr.c = 1;
    break;

  case OP_SED: /* Set Decimal Mode */
    cpu->sr.d = 1;
    break;

  case OP_SEI: /* Set Interrupt Disable Status */
    cpu->sr.i = 1;
    break;

  case OP_STA: /* Store Accumulator in Memory */
    mem_write(mem, address, cpu->a);
    break;

  case OP_STX: /* Store Index X in Memory */
    mem_write(mem, address, cpu->x);
    break;

  case OP_STY: /* Store Index Y in Memory */
    mem_write(mem, address, cpu->y);
    break;

  case OP_TAX: /* Transfer Accumulator to Index X */
    cpu->x = cpu->a;
    cpu_flag_negative_other(cpu, cpu->x);
    cpu_flag_zero_other(cpu, cpu->x);
    break;

  case OP_TAY: /* Transfer Accumulator to Index Y */
    cpu->y = cpu->a;
    cpu_flag_negative_other(cpu, cpu->y);
    cpu_flag_zero_other(cpu, cpu->y);
    break;

  case OP_TSX: /* Transfer Stack Pointer to Index X */
    cpu->x = cpu->sp;
    cpu_flag_negative_other(cpu, cpu->x);
    cpu_flag_zero_other(cpu, cpu->x);
    break;

  case OP_TXA: /* Transfer Index X to Accumulator */
    cpu->a = cpu->x;
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_TXS: /* Transfer Index X to Stack Pointer */
    cpu->sp = cpu->x;
    break;

  case OP_TYA: /* Transfer Index Y to Accumulator */
    cpu->a = cpu->y;
    cpu_flag_negative_other(cpu, cpu->a);
    cpu_flag_zero_other(cpu, cpu->a);
    break;

  case OP_NONE:
  default:
    if (cpu_trap_opcode_handler != NULL) {
      if (true == (cpu_trap_opcode_handler)(opcode, cpu, mem)) {
        /* Request was handled...  */
        break;
      }
    }
    panic("Panic! Unhandled opcode: %02x\n", opcode);
    break;
  }
}



