#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mos6510.h"
#include "mos6510_trace.h"
#include "mem.h"
#include "cia.h"
#include "serial_bus.h"
#include "disk.h"
#include "panic.h"
#include "debugger.h"

#define DEBUGGER_ARGS 3
#define DEBUGGER_BREAKPOINT_MAX 5
#define DEBUGGER_STACK_TRACE_SIZE 128

typedef struct debugger_breakpoint_s {
  uint16_t address;
  bool read;
  bool write;
  bool execute;
} debugger_breakpoint_t;

typedef struct debugger_stack_trace_s {
  uint16_t from;
  uint16_t to;
} debugger_stack_trace_t;

debugger_breakpoint_t debugger_breakpoint[DEBUGGER_BREAKPOINT_MAX];

static debugger_stack_trace_t debugger_stack_trace[DEBUGGER_STACK_TRACE_SIZE];
static int debugger_stack_trace_index = 0;
static char *debugger_symtab[UINT16_MAX + 1];



static void debugger_breakpoint_list(void)
{
  int i;
  for (i = 0; i < DEBUGGER_BREAKPOINT_MAX; i++) {
    if (debugger_breakpoint[i].read == true) {
      fprintf(stdout, "%d: 0x%04x (r)\n", i+1, debugger_breakpoint[i].address);
    }
    if (debugger_breakpoint[i].write == true) {
      fprintf(stdout, "%d: 0x%04x (w)\n", i+1, debugger_breakpoint[i].address);
    }
    if (debugger_breakpoint[i].execute == true) {
      fprintf(stdout, "%d: 0x%04x (x)\n", i+1, debugger_breakpoint[i].address);
    }
  }
}



static void debugger_breakpoint_add(uint16_t address, int type)
{
  int i;
  for (i = 0; i < DEBUGGER_BREAKPOINT_MAX; i++) {
    if (debugger_breakpoint[i].read == false &&
        debugger_breakpoint[i].write == false &&
        debugger_breakpoint[i].execute == false) {

      debugger_breakpoint[i].address = address;
      switch (type) {
      case 0:
        debugger_breakpoint[i].read = true;
        break;

      case 1:
        debugger_breakpoint[i].write = true;
        break;

      case 2:
        debugger_breakpoint[i].execute = true;
        break;

      default:
        break;
      }
      return;
    }
  }

  fprintf(stdout, "Max breakpoints reached!\n");
}



static void debugger_breakpoint_del(int breakpoint_no)
{
  breakpoint_no--; /* Convert to zero-indexed. */
  if (breakpoint_no >= 0 && breakpoint_no < DEBUGGER_BREAKPOINT_MAX) {
    debugger_breakpoint[breakpoint_no].address = 0;
    debugger_breakpoint[breakpoint_no].read = false;
    debugger_breakpoint[breakpoint_no].write = false;
    debugger_breakpoint[breakpoint_no].execute = false;
  }
}



static void debugger_help(void)
{
  fprintf(stdout, "Debugger Commands:\n");
  fprintf(stdout, "  q              - Quit\n");
  fprintf(stdout, "  ? | h          - Help\n");
  fprintf(stdout, "  c              - Continue\n");
  fprintf(stdout, "  s              - Step\n");
  fprintf(stdout, "  r              - CPU Reset\n");
  fprintf(stdout, "  t              - Dump CPU Trace\n");
  fprintf(stdout, "  y              - Dump Stack Trace\n");
  fprintf(stdout, "  z              - Dump Zero Page\n");
  fprintf(stdout, "  k              - Dump Stack\n");
  fprintf(stdout, "  p              - Dump Program Area\n");
  fprintf(stdout, "  d <addr> [end] - Dump Memory\n");
  fprintf(stdout, "  bp             - Breakpoint Print\n");
  fprintf(stdout, "  br <addr>      - Breakpoint on Memory Read\n");
  fprintf(stdout, "  bw <addr>      - Breakpoint on Memory Write\n");
  fprintf(stdout, "  bx <addr>      - Breakpoint on Execute\n");
  fprintf(stdout, "  bd <no>        - Breakpoint Delete\n");
  fprintf(stdout, "  l <prg>        - Load PRG\n");
  fprintf(stdout, "  8 <d64>        - Load D64 in Device #8\n");
  fprintf(stdout, "  a              - Dump CIA Registers\n");
  fprintf(stdout, "  v              - Dump VIC-II Registers\n");
  fprintf(stdout, "  e              - Dump Serial Bus Info\n");
  fprintf(stdout, "  f              - Dump Disk Info\n");
  fprintf(stdout, "  w              - Toggle Warp Mode\n");
}



void debugger_init(void)
{
  int i;
  for (i = 0; i < DEBUGGER_BREAKPOINT_MAX; i++) {
    debugger_breakpoint[i].address = 0;
    debugger_breakpoint[i].read = false;
    debugger_breakpoint[i].write = false;
    debugger_breakpoint[i].execute = false;
  }
}



bool debugger(mos6510_t *cpu, mem_t *mem, serial_bus_t *serial_bus)
{
  char input[128];
  char *argv[DEBUGGER_ARGS];
  int argc;
  int value1, value2;

  fprintf(stdout, "\n");
  while (1) {
    fprintf(stdout, "%04x> ", cpu->pc);

    if (fgets(input, sizeof(input), stdin) == NULL) {
      continue;
    }

    if ((strlen(input) > 0) && (input[strlen(input) - 1] == '\n')) {
      input[strlen(input) - 1] = '\0'; /* Strip newline. */
    }

    argv[0] = strtok(input, " ");
    if (argv[0] == NULL) {
      continue;
    }

    for (argc = 1; argc < DEBUGGER_ARGS; argc++) {
      argv[argc] = strtok(NULL, " ");
      if (argv[argc] == NULL) {
        break;
      }
    }

    if (strncmp(argv[0], "q", 1) == 0) {
      exit(EXIT_SUCCESS);

    } else if (strncmp(argv[0], "?", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "h", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "c", 1) == 0) {
      return false;

    } else if (strncmp(argv[0], "s", 1) == 0) {
      return true;

    } else if (strncmp(argv[0], "r", 1) == 0) {
      mos6510_reset(cpu, mem);
      return false;

    } else if (strncmp(argv[0], "t", 1) == 0) {
      fprintf(stdout, "CPU Trace:\n");
      mos6510_trace_dump(stdout);

    } else if (strncmp(argv[0], "y", 1) == 0) {
      fprintf(stdout, "Stack Trace:\n");
      debugger_stack_trace_dump(stdout);

    } else if (strncmp(argv[0], "z", 1) == 0) {
      fprintf(stdout, "Zero Page:\n");
      mem_ram_dump(stdout, mem, 0, 0xFF);

    } else if (strncmp(argv[0], "k", 1) == 0) {
      fprintf(stdout, "Stack:\n");
      mem_ram_dump(stdout, mem, MEM_PAGE_STACK, MEM_PAGE_STACK + 0xFF);

    } else if (strncmp(argv[0], "p", 1) == 0) {
      fprintf(stdout, "Program Area:\n");
      mem_ram_dump(stdout, mem, 0x800, 0x8FF);

    } else if (strncmp(argv[0], "d", 1) == 0) {
      if (argc >= 3) {
        sscanf(argv[1], "%4x", &value1);
        sscanf(argv[2], "%4x", &value2);
        mem_ram_dump(stdout, mem, (uint16_t)value1, (uint16_t)value2);
      } else if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        value2 = value1 + 0xFF;
        if (value2 > 0xFFFF) {
          value2 = 0xFFFF; /* Truncate */
        }
        mem_ram_dump(stdout, mem, (uint16_t)value1, (uint16_t)value2);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "bp", 2) == 0) {
      fprintf(stdout, "Breakpoints:\n");
      debugger_breakpoint_list();

    } else if (strncmp(argv[0], "br", 2) == 0) {
      if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        debugger_breakpoint_add(value1, 0);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "bw", 2) == 0) {
      if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        debugger_breakpoint_add(value1, 1);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "bx", 2) == 0) {
      if (argc >= 2) {
        sscanf(argv[1], "%4x", &value1);
        debugger_breakpoint_add(value1, 2);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "bd", 2) == 0) {
      if (argc >= 2) {
        sscanf(argv[1], "%d", &value1);
        debugger_breakpoint_del(value1);
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "l", 1) == 0) {
      if (argc >= 2) {
        if (mem_load_prg(mem, argv[1]) != 0) {
          fprintf(stdout, "Loading of '%s' failed!\n", argv[1]);
        }
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "8", 1) == 0) {
      if (argc >= 2) {
        if (disk_load_d64(8, argv[1]) != 0) {
          fprintf(stdout, "Loading of '%s' failed!\n", argv[1]);
        }
      } else {
        fprintf(stdout, "Missing argument!\n");
      }

    } else if (strncmp(argv[0], "a", 1) == 0) {
      fprintf(stdout, "CIA Dump:\n");
      cia_dump(stdout, (cia_t *)mem->cia1);
      cia_dump(stdout, (cia_t *)mem->cia2);

    } else if (strncmp(argv[0], "v", 1) == 0) {
      fprintf(stdout, "VIC-II Dump:\n");
      mem_vic2_dump(stdout, mem);

    } else if (strncmp(argv[0], "e", 1) == 0) {
      fprintf(stdout, "Serial Bus Dump:\n");
      serial_bus_dump(stdout, serial_bus);

    } else if (strncmp(argv[0], "f", 1) == 0) {
      fprintf(stdout, "Disk Dump:\n");
      disk_dump(stdout);

    } else if (strncmp(argv[0], "w", 1) == 0) {
      if (warp_mode) {
        fprintf(stdout, "Warp Mode: Off\n");
        warp_mode = false;
      } else {
        fprintf(stdout, "Warp Mode: On\n");
        warp_mode = true;
      }

    }
  }
}



void debugger_mem_read(uint16_t address)
{
  int i;
  for (i = 0; i < DEBUGGER_BREAKPOINT_MAX; i++) {
    if (debugger_breakpoint[i].read == true  &&
        debugger_breakpoint[i].address == address) {
      debugger_break = true;
    }
  }
}



void debugger_mem_write(uint16_t address, uint8_t value)
{
  (void)value; /* Not currently used. */
  int i;
  for (i = 0; i < DEBUGGER_BREAKPOINT_MAX; i++) {
    if (debugger_breakpoint[i].write == true  &&
        debugger_breakpoint[i].address == address) {
      debugger_break = true;
    }
  }
}



void debugger_mem_execute(uint16_t address)
{
  int i;
  for (i = 0; i < DEBUGGER_BREAKPOINT_MAX; i++) {
    if (debugger_breakpoint[i].execute == true  &&
        debugger_breakpoint[i].address == address) {
      debugger_break = true;
    }
  }
}



void debugger_stack_trace_init(void)
{
  memset(debugger_symtab, 0, (UINT16_MAX + 1) * (sizeof(char *)));
  debugger_symtab[0xED09] = "TALK";
  debugger_symtab[0xED0C] = "LISTN";
  debugger_symtab[0xED11] = "LIST1";
  debugger_symtab[0xED36] = "ISOURA";
  debugger_symtab[0xED40] = "ISOUR";
  debugger_symtab[0xEDB9] = "SECND";
  debugger_symtab[0xEDBE] = "SCATN";
  debugger_symtab[0xEDC7] = "TKSA";
  debugger_symtab[0xEDCC] = "TKATN";
  debugger_symtab[0xEDFE] = "UNLSN";
  debugger_symtab[0xEE13] = "ACPTR";
  debugger_symtab[0xEE85] = "CLKHI";
  debugger_symtab[0xEE8E] = "CLKLO";
  debugger_symtab[0xEE97] = "DATAHI";
  debugger_symtab[0xEEA0] = "DATALO";
  debugger_symtab[0xEEA9] = "DEBPIA";
  debugger_symtab[0xEEB3] = "W1MS";
  debugger_symtab[0xF3D5] = "OPENI";
  debugger_symtab[0xFFCF] = "BASIN";
}



void debugger_stack_trace_add(uint16_t from, uint16_t to)
{
  debugger_stack_trace[debugger_stack_trace_index].from = from;
  debugger_stack_trace[debugger_stack_trace_index].to = to;

  if (debugger_stack_trace_index < (DEBUGGER_STACK_TRACE_SIZE - 1)) {
    debugger_stack_trace_index++;
  }
}



void debugger_stack_trace_rem(void)
{
  if (debugger_stack_trace_index > 0) {
    debugger_stack_trace_index--;
  }
}



void debugger_stack_trace_dump(FILE *fh)
{
  int i;
  for (i = 0; i < debugger_stack_trace_index; i++) {
    fprintf(fh, "%04x -> %04x",
      debugger_stack_trace[i].from,
      debugger_stack_trace[i].to);
    if (debugger_symtab[debugger_stack_trace[i].to] != NULL) {
      fprintf(fh, " (%s)", debugger_symtab[debugger_stack_trace[i].to]);
    }
    fprintf(fh, "\n");
  }
}



