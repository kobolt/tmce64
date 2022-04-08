#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mos6510.h"
#include "mem.h"
#include "panic.h"



static mos6510_opcode_handler_t mos6510_trap_opcode_handler = NULL;

void mos6510_trap_opcode(uint8_t opcode, mos6510_opcode_handler_t handler)
{
  (void)opcode; /* NOTE: Currently just one handler for all opcodes. */
  mos6510_trap_opcode_handler = handler;
}



static uint8_t mos6510_status_get(mos6510_t *cpu, bool b_flag)
{
  return ((cpu->sr.n << 7) +
          (cpu->sr.v << 6) +
          (1         << 5) +
          (b_flag    << 4) +
          (cpu->sr.d << 3) +
          (cpu->sr.i << 2) +
          (cpu->sr.z << 1) +
           cpu->sr.c);
}

static void mos6510_status_set(mos6510_t *cpu, uint8_t flags)
{
  cpu->sr.n = (flags >> 7) & 0x1;
  cpu->sr.v = (flags >> 6) & 0x1;
  cpu->sr.b = 0;
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



static inline void flag_zero_other(mos6510_t *cpu, uint8_t value)
{
  if (value == 0) {
    cpu->sr.z = 1;
  } else {
    cpu->sr.z = 0;
  }
}

static inline void flag_negative_other(mos6510_t *cpu, uint8_t value)
{
  if ((value >> 7) == 1) {
    cpu->sr.n = 1;
  } else {
    cpu->sr.n = 0;
  }
}

static inline void flag_zero_compare(mos6510_t *cpu, uint8_t a, uint8_t b)
{
  if (a == b) {
    cpu->sr.z = 1;
  } else {
    cpu->sr.z = 0;
  }
}

static inline void flag_negative_compare(mos6510_t *cpu, uint8_t a, uint8_t b)
{
  if (((uint8_t)(a - b) >> 7) == 1) {
    cpu->sr.n = 1;
  } else {
    cpu->sr.n = 0;
  }
}

static inline void flag_carry_compare(mos6510_t *cpu, uint8_t a, uint8_t b)
{
  if (a >= b) {
    cpu->sr.c = 1;
  } else {
    cpu->sr.c = 0;
  }
}

static inline bool flag_carry_add(mos6510_t *cpu, uint8_t value)
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

static inline bool flag_carry_sub(mos6510_t *cpu, uint8_t value)
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

static inline void flag_overflow_add(mos6510_t *cpu, uint8_t a, uint8_t b)
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

static inline void flag_overflow_sub(mos6510_t *cpu, uint8_t a, uint8_t b)
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

static inline void flag_overflow_bit(mos6510_t *cpu, uint8_t value)
{
  if (((value >> 6) & 0x1) == 1) {
    cpu->sr.v = 1;
  } else {
    cpu->sr.v = 0;
  }
}



#define OP_PROLOGUE_ABS \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256;

#define OP_PROLOGUE_ABSX \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256; \
  absolute += cpu->x;

#define OP_PROLOGUE_ABSY \
  uint16_t absolute; \
  absolute  = mem_read(mem, cpu->pc++); \
  absolute += mem_read(mem, cpu->pc++) * 256; \
  absolute += cpu->y; \

#define OP_PROLOGUE_ZP \
  uint8_t zeropage; \
  zeropage = mem_read(mem, cpu->pc++); \

#define OP_PROLOGUE_ZPX \
  uint8_t zeropage; \
  zeropage  = mem_read(mem, cpu->pc++); \
  zeropage += cpu->x;

#define OP_PROLOGUE_ZPY \
  uint8_t zeropage; \
  zeropage  = mem_read(mem, cpu->pc++); \
  zeropage += cpu->y;

#define OP_PROLOGUE_ZPYI \
  uint8_t zeropage; \
  uint16_t absolute; \
  zeropage  = mem_read(mem, cpu->pc++); \
  absolute  = mem_read(mem, zeropage); \
  zeropage += 1; \
  absolute += mem_read(mem, zeropage) * 256; \
  absolute += cpu->y;

#define OP_PROLOGUE_ZPIX \
  uint8_t zeropage; \
  uint16_t absolute; \
  zeropage  = mem_read(mem, cpu->pc++); \
  zeropage += cpu->x; \
  absolute  = mem_read(mem, zeropage); \
  zeropage += 1; \
  absolute += mem_read(mem, zeropage) * 256;



static inline void mos6510_logic_adc(mos6510_t *cpu, uint8_t value)
{
  uint8_t initial;
  bool bit;
  if (cpu->sr.d == 1) {
    value = dec_to_bin(value);
    cpu->a = dec_to_bin(cpu->a);
  }
  initial = cpu->a;
  bit = flag_carry_add(cpu, value);
  cpu->a += value;
  cpu->a += cpu->sr.c;
  if (cpu->sr.d == 1) {
    if (cpu->a > 99) {
      cpu->a %= 100;
    }
    cpu->a = bin_to_dec(cpu->a);
  }
  cpu->sr.c = bit;
  flag_overflow_add(cpu, initial, value);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static inline void mos6510_logic_sbc(mos6510_t *cpu, uint8_t value)
{
  uint8_t initial;
  bool bit;
  if (cpu->sr.d == 1) {
    value = dec_to_bin(value);
    cpu->a = dec_to_bin(cpu->a);
  }
  initial = cpu->a;
  bit = flag_carry_sub(cpu, value);
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
  flag_overflow_sub(cpu, initial, value);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}



static void op_adc_imm(mos6510_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_adc(cpu, value);
}

static void op_adc_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_adc(cpu, value);
}

static void op_and_imm(mos6510_t *cpu, mem_t *mem)
{
  cpu->a &= mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a &= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a &= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_and_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a &= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_asl_accu(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b10000000;
  value = value << 1;
  cpu->a = value;
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_asl_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_bcc(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.c == 0) {
    cpu->pc += relative;
  }
}

static void op_bcs(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.c == 1) {
    cpu->pc += relative;
  }
}

static void op_beq(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.z == 1) {
    cpu->pc += relative;
  }
}

static void op_bit_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_overflow_bit(cpu, value);
  flag_negative_other(cpu, value);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bit_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_overflow_bit(cpu, value);
  flag_negative_other(cpu, value);
  value &= cpu->a;
  flag_zero_other(cpu, value);
}

static void op_bmi(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.n == 1) {
    cpu->pc += relative;
  }
}

static void op_bne(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.z == 0) {
    cpu->pc += relative;
  }
}

static void op_bpl(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.n == 0) {
    cpu->pc += relative;
  }
}

static void op_brk(mos6510_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc + 1) / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc + 1) % 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, mos6510_status_get(cpu, 1));
  cpu->sr.i = 1;
  cpu->sr.b = 1;
  cpu->pc  = mem_read(mem, MOS6510_VECTOR_IRQ_LOW);
  cpu->pc += mem_read(mem, MOS6510_VECTOR_IRQ_HIGH) * 256;
}

static void op_bvc(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.v == 0) {
    cpu->pc += relative;
  }
}

static void op_bvs(mos6510_t *cpu, mem_t *mem)
{
  int8_t relative = mem_read(mem, cpu->pc++);
  if (cpu->sr.v == 1) {
    cpu->pc += relative;
  }
}

static void op_clc(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.c = 0;
}

static void op_cld(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.d = 0;
}

static void op_cli(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.i = 0;
}

static void op_clv(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.v = 0;
}

static void op_cmp_imm(mos6510_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cmp_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->a, value);
  flag_zero_compare(cpu, cpu->a, value);
  flag_carry_compare(cpu, cpu->a, value);
}

static void op_cpx_imm(mos6510_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  flag_negative_compare(cpu, cpu->x, value);
  flag_zero_compare(cpu, cpu->x, value);
  flag_carry_compare(cpu, cpu->x, value);
}

static void op_cpx_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->x, value);
  flag_zero_compare(cpu, cpu->x, value);
  flag_carry_compare(cpu, cpu->x, value);
}

static void op_cpx_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->x, value);
  flag_zero_compare(cpu, cpu->x, value);
  flag_carry_compare(cpu, cpu->x, value);
}

static void op_cpy_imm(mos6510_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  flag_negative_compare(cpu, cpu->y, value);
  flag_zero_compare(cpu, cpu->y, value);
  flag_carry_compare(cpu, cpu->y, value);
}

static void op_cpy_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  flag_negative_compare(cpu, cpu->y, value);
  flag_zero_compare(cpu, cpu->y, value);
  flag_carry_compare(cpu, cpu->y, value);
}

static void op_cpy_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  flag_negative_compare(cpu, cpu->y, value);
  flag_zero_compare(cpu, cpu->y, value);
  flag_carry_compare(cpu, cpu->y, value);
}

static void op_dec_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  value -= 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dec_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  value -= 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dec_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value -= 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dec_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  value -= 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_dex(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x--;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_dey(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->y--;
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_eor_imm(mos6510_t *cpu, mem_t *mem)
{
  cpu->a ^= mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a ^= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a ^= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_eor_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a ^= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_inc_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  value += 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inc_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  value += 1;
  mem_write(mem, absolute, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inc_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  value += 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inc_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  value += 1;
  mem_write(mem, zeropage, value);
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_inx(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x++;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_iny(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->y++;
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_jmp_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->pc = absolute;
}

static void op_jmp_absi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint16_t address = mem_read(mem, absolute);
  absolute += 1;
  address += mem_read(mem, absolute) * 256;
  cpu->pc = address;
}

static void op_jsr(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc - 1) / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, (cpu->pc - 1) % 256);
  cpu->pc = absolute;
}

static void op_lda_imm(mos6510_t *cpu, mem_t *mem)
{
  cpu->a = mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_lda_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ldx_imm(mos6510_t *cpu, mem_t *mem)
{
  cpu->x = mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->x = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  cpu->x = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->x = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldx_zpy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPY
  cpu->x = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_ldy_imm(mos6510_t *cpu, mem_t *mem)
{
  cpu->y = mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->y = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  cpu->y = mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->y = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_ldy_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->y = mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_lsr_accu(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b00000001;
  value = value >> 1;
  cpu->a = value;
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_lsr_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_nop(mos6510_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
}

static void op_ora_imm(mos6510_t *cpu, mem_t *mem)
{
  cpu->a |= mem_read(mem, cpu->pc++);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  cpu->a |= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  cpu->a |= mem_read(mem, zeropage);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_ora_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  cpu->a |= mem_read(mem, absolute);
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_pha(mos6510_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu->a);
}

static void op_php(mos6510_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, mos6510_status_get(cpu, 1));
}

static void op_pla(mos6510_t *cpu, mem_t *mem)
{
  cpu->a = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_plp(mos6510_t *cpu, mem_t *mem)
{
  mos6510_status_set(cpu, mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)));
}

static void op_rol_accu(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->sr.c == 1) {
    value |= 0b00000001;
  }
  cpu->a = value;
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->sr.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->sr.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->sr.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rol_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b10000000;
  value = value << 1;
  if (cpu->sr.c == 1) {
    value |= 0b00000001;
  }
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_accu(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t value = cpu->a;
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->sr.c == 1) {
    value |= 0b10000000;
  }
  cpu->a = value;
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->sr.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->sr.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, absolute, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->sr.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_ror_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  bool bit = value & 0b00000001;
  value = value >> 1;
  if (cpu->sr.c == 1) {
    value |= 0b10000000;
  }
  mem_write(mem, zeropage, value);
  cpu->sr.c = bit;
  flag_negative_other(cpu, value);
  flag_zero_other(cpu, value);
}

static void op_rti(mos6510_t *cpu, mem_t *mem)
{
  mos6510_status_set(cpu, mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)));
  cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
  cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)) * 256;
}

static void op_rts(mos6510_t *cpu, mem_t *mem)
{
  cpu->pc  = mem_read(mem, MEM_PAGE_STACK + (++cpu->sp));
  cpu->pc += mem_read(mem, MEM_PAGE_STACK + (++cpu->sp)) * 256;
  cpu->pc += 1;
}

static void op_sbc_imm(mos6510_t *cpu, mem_t *mem)
{
  uint8_t value = mem_read(mem, cpu->pc++);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  uint8_t value = mem_read(mem, zeropage);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  uint8_t value = mem_read(mem, zeropage);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_sbc(cpu, value);
}

static void op_sbc_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  uint8_t value = mem_read(mem, absolute);
  mos6510_logic_sbc(cpu, value);
}

static void op_sec(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.c = 1;
}

static void op_sed(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.d = 1;
}

static void op_sei(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sr.i = 1;
}

static void op_sta_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_absx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSX
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_absy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABSY
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, cpu->a);
}

static void op_sta_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  mem_write(mem, zeropage, cpu->a);
}

static void op_sta_zpyi(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPYI
  mem_write(mem, absolute, cpu->a);
}

static void op_sta_zpix(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPIX
  mem_write(mem, absolute, cpu->a);
}

static void op_stx_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, cpu->x);
}

static void op_stx_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, cpu->x);
}

static void op_stx_zpy(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPY
  mem_write(mem, zeropage, cpu->x);
}

static void op_sty_abs(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ABS
  mem_write(mem, absolute, cpu->y);
}

static void op_sty_zp(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZP
  mem_write(mem, zeropage, cpu->y);
}

static void op_sty_zpx(mos6510_t *cpu, mem_t *mem)
{
  OP_PROLOGUE_ZPX
  mem_write(mem, zeropage, cpu->y);
}

static void op_tax(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x = cpu->a;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_tay(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->y = cpu->a;
  flag_negative_other(cpu, cpu->y);
  flag_zero_other(cpu, cpu->y);
}

static void op_tsx(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->x = cpu->sp;
  flag_negative_other(cpu, cpu->x);
  flag_zero_other(cpu, cpu->x);
}

static void op_txa(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->a = cpu->x;
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}

static void op_txs(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->sp = cpu->x;
}

static void op_tya(mos6510_t *cpu, mem_t *mem)
{
  (void)mem;
  cpu->a = cpu->y;
  flag_negative_other(cpu, cpu->a);
  flag_zero_other(cpu, cpu->a);
}



static void op_none(mos6510_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  opcode = mem_read(mem, cpu->pc - 1);

  if (mos6510_trap_opcode_handler != NULL) {
    if (true == (mos6510_trap_opcode_handler)(opcode, cpu, mem)) {
      /* Request was handled...  */
      return;
    }
  }
  panic("Panic! Unhandled opcode: %02x\n", opcode);
}



typedef void (*mos6510_operation_func_t)(mos6510_t *, mem_t *);

static mos6510_operation_func_t opcode_function[UINT8_MAX + 1] = {
  op_brk,      op_ora_zpix, op_none,     op_none, /* 0x00 -> 0x03 */
  op_none,     op_ora_zp,   op_asl_zp,   op_none, /* 0x04 -> 0x07 */
  op_php,      op_ora_imm,  op_asl_accu, op_none, /* 0x08 -> 0x0B */
  op_none,     op_ora_abs,  op_asl_abs,  op_none, /* 0x0C -> 0x0F */
  op_bpl,      op_ora_zpyi, op_none,     op_none, /* 0x10 -> 0x13 */
  op_none,     op_ora_zpx,  op_asl_zpx,  op_none, /* 0x14 -> 0x17 */
  op_clc,      op_ora_absy, op_none,     op_none, /* 0x18 -> 0x1B */
  op_none,     op_ora_absx, op_asl_absx, op_none, /* 0x1C -> 0x1F */
  op_jsr,      op_and_zpix, op_none,     op_none, /* 0x20 -> 0x23 */
  op_bit_zp,   op_and_zp,   op_rol_zp,   op_none, /* 0x24 -> 0x27 */
  op_plp,      op_and_imm,  op_rol_accu, op_none, /* 0x28 -> 0x2B */
  op_bit_abs,  op_and_abs,  op_rol_abs,  op_none, /* 0x2C -> 0x2F */
  op_bmi,      op_and_zpyi, op_none,     op_none, /* 0x30 -> 0x33 */
  op_none,     op_and_zpx,  op_rol_zpx,  op_none, /* 0x34 -> 0x37 */
  op_sec,      op_and_absy, op_none,     op_none, /* 0x38 -> 0x3B */
  op_none,     op_and_absx, op_rol_absx, op_none, /* 0x3C -> 0x3F */
  op_rti,      op_eor_zpix, op_none,     op_none, /* 0x40 -> 0x43 */
  op_none,     op_eor_zp,   op_lsr_zp,   op_none, /* 0x44 -> 0x47 */
  op_pha,      op_eor_imm,  op_lsr_accu, op_none, /* 0x48 -> 0x4B */
  op_jmp_abs,  op_eor_abs,  op_lsr_abs,  op_none, /* 0x4C -> 0x4F */
  op_bvc,      op_eor_zpyi, op_none,     op_none, /* 0x50 -> 0x53 */
  op_none,     op_eor_zpx,  op_lsr_zpx,  op_none, /* 0x54 -> 0x57 */
  op_cli,      op_eor_absy, op_none,     op_none, /* 0x58 -> 0x5B */
  op_none,     op_eor_absx, op_lsr_absx, op_none, /* 0x5C -> 0x5F */
  op_rts,      op_adc_zpix, op_none,     op_none, /* 0x60 -> 0x63 */
  op_none,     op_adc_zp,   op_ror_zp,   op_none, /* 0x64 -> 0x67 */
  op_pla,      op_adc_imm,  op_ror_accu, op_none, /* 0x68 -> 0x6B */
  op_jmp_absi, op_adc_abs,  op_ror_abs,  op_none, /* 0x6C -> 0x6F */
  op_bvs,      op_adc_zpyi, op_none,     op_none, /* 0x70 -> 0x73 */
  op_none,     op_adc_zpx,  op_ror_zpx,  op_none, /* 0x74 -> 0x77 */
  op_sei,      op_adc_absy, op_none,     op_none, /* 0x78 -> 0x7B */
  op_none,     op_adc_absx, op_ror_absx, op_none, /* 0x7C -> 0x7F */
  op_none,     op_sta_zpix, op_none,     op_none, /* 0x80 -> 0x83 */
  op_sty_zp,   op_sta_zp,   op_stx_zp,   op_none, /* 0x84 -> 0x87 */
  op_dey,      op_none,     op_txa,      op_none, /* 0x88 -> 0x8B */
  op_sty_abs,  op_sta_abs,  op_stx_abs,  op_none, /* 0x8C -> 0x8F */
  op_bcc,      op_sta_zpyi, op_none,     op_none, /* 0x90 -> 0x93 */
  op_sty_zpx,  op_sta_zpx,  op_stx_zpy,  op_none, /* 0x94 -> 0x97 */
  op_tya,      op_sta_absy, op_txs,      op_none, /* 0x98 -> 0x9B */
  op_none,     op_sta_absx, op_none,     op_none, /* 0x9C -> 0x9F */
  op_ldy_imm,  op_lda_zpix, op_ldx_imm,  op_none, /* 0xA0 -> 0xA3 */
  op_ldy_zp,   op_lda_zp,   op_ldx_zp,   op_none, /* 0xA4 -> 0xA7 */
  op_tay,      op_lda_imm,  op_tax,      op_none, /* 0xA8 -> 0xAB */
  op_ldy_abs,  op_lda_abs,  op_ldx_abs,  op_none, /* 0xAC -> 0xAF */
  op_bcs,      op_lda_zpyi, op_none,     op_none, /* 0xB0 -> 0xB3 */
  op_ldy_zpx,  op_lda_zpx,  op_ldx_zpy,  op_none, /* 0xB4 -> 0xB7 */
  op_clv,      op_lda_absy, op_tsx,      op_none, /* 0xB8 -> 0xBB */
  op_ldy_absx, op_lda_absx, op_ldx_absy, op_none, /* 0xBC -> 0xBF */
  op_cpy_imm,  op_cmp_zpix, op_none,     op_none, /* 0xC0 -> 0xC3 */
  op_cpy_zp,   op_cmp_zp,   op_dec_zp,   op_none, /* 0xC4 -> 0xC7 */
  op_iny,      op_cmp_imm,  op_dex,      op_none, /* 0xC8 -> 0xCB */
  op_cpy_abs,  op_cmp_abs,  op_dec_abs,  op_none, /* 0xCC -> 0xCF */
  op_bne,      op_cmp_zpyi, op_none,     op_none, /* 0xD0 -> 0xD3 */
  op_none,     op_cmp_zpx,  op_dec_zpx,  op_none, /* 0xD4 -> 0xD7 */
  op_cld,      op_cmp_absy, op_none,     op_none, /* 0xD8 -> 0xDB */
  op_none,     op_cmp_absx, op_dec_absx, op_none, /* 0xDC -> 0xDF */
  op_cpx_imm,  op_sbc_zpix, op_none,     op_none, /* 0xE0 -> 0xE3 */
  op_cpx_zp,   op_sbc_zp,   op_inc_zp,   op_none, /* 0xE4 -> 0xE7 */
  op_inx,      op_sbc_imm,  op_nop,      op_none, /* 0xE8 -> 0xEB */
  op_cpx_abs,  op_sbc_abs,  op_inc_abs,  op_none, /* 0xEC -> 0xEF */
  op_beq,      op_sbc_zpyi, op_none,     op_none, /* 0xF0 -> 0xF3 */
  op_none,     op_sbc_zpx,  op_inc_zpx,  op_none, /* 0xF4 -> 0xF7 */
  op_sed,      op_sbc_absy, op_none,     op_none, /* 0xF8 -> 0xFB */
  op_none,     op_sbc_absx, op_inc_absx, op_none, /* 0xFC -> 0xFF */
};



void mos6510_execute(mos6510_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  opcode = mem_read(mem, cpu->pc++);
  (opcode_function[opcode])(cpu, mem);
}



void mos6510_reset(mos6510_t *cpu, mem_t *mem)
{
  cpu->pc  = mem_read(mem, MOS6510_VECTOR_RESET_LOW);
  cpu->pc += mem_read(mem, MOS6510_VECTOR_RESET_HIGH) * 256;
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->sp = 0xFD;
  cpu->sr.n = 0;
  cpu->sr.v = 0;
  cpu->sr.b = 0;
  cpu->sr.d = 0;
  cpu->sr.i = 1;
  cpu->sr.z = 0;
  cpu->sr.c = 0;
}



void mos6510_nmi(mos6510_t *cpu, mem_t *mem)
{
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu->pc / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu->pc % 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, mos6510_status_get(cpu, 0));
  cpu->sr.i = 1;
  cpu->pc  = mem_read(mem, MOS6510_VECTOR_NMI_LOW);
  cpu->pc += mem_read(mem, MOS6510_VECTOR_NMI_HIGH) * 256;
}



void mos6510_irq(mos6510_t *cpu, mem_t *mem)
{
  if (cpu->sr.i == 1) {
    return; /* Masked. */
  }
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu->pc / 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, cpu->pc % 256);
  mem_write(mem, MEM_PAGE_STACK + cpu->sp--, mos6510_status_get(cpu, 0));
  cpu->sr.i = 1;
  cpu->pc  = mem_read(mem, MOS6510_VECTOR_IRQ_LOW);
  cpu->pc += mem_read(mem, MOS6510_VECTOR_IRQ_HIGH) * 256;
}



