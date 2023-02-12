#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>

#include <unistd.h>
#include <sys/time.h>
#include <limits.h>

#include "mos6510.h"
#include "mos6510_trace.h"
#include "mem.h"
#include "cia.h"
#include "vic.h"
#include "serial_bus.h"
#include "disk.h"
#include "panic.h"
#include "console.h"
#include "joystick.h"
#include "debugger.h"
#include "test.h"
#ifdef RESID
#include "resid.h"
#endif



#define DEFAULT_ROM_DIRECTORY "/usr/share/vice/C64/"
#define KERNAL_ROM "kernal"
#define BASIC_ROM "basic"
#define CHAR_ROM "chargen"



static mos6510_t cpu;
static mem_t mem;
static cia_t cia1;
static cia_t cia2;
static vic_t vic;
static serial_bus_t bus;

bool debugger_break = false;
bool warp_mode = false;
static char panic_msg[80];
static char *pending_prg = NULL;



static void sig_handler(int sig)
{
  switch (sig) {
  case SIGALRM:
    return; /* Ignore */

  case SIGINT:
    debugger_break = true;
    return;
  }
}



void panic(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(panic_msg, sizeof(panic_msg), format, args);
  va_end(args);

  debugger_break = true;
}



static void display_help(const char *progname)
{
  fprintf(stdout, "Usage: %s <options> [prg]\n", progname);
  fprintf(stdout, "Options:\n"
     "  -h        Display this help.\n"
     "  -b        Break into debugger on start.\n"
     "  -w        Warp mode, disable real C64 speed emulation.\n"
     "  -8 FILE   Load D64 FILE into disk drive device #8.\n"
     "  -r DIR    Load ROM file from DIR instead of default location.\n"
     "  -l        Run Lorenz CPU test.\n"
     "  -d        Run Dormann CPU test.\n"
     "\n");
  fprintf(stdout,
    "Specify a PRG file to load it automatically on start.\n"
    "Using Ctrl+C will break into debugger, use 'q' from there to quit.\n"
    "\n");
}



int main(int argc, char *argv[])
{
  int c;
  bool dormann_test = false;
  bool lorenz_test = false;
  char *rom_directory = NULL;
  char *d64_filename = NULL;
  char rom_path[PATH_MAX];
  int sync_cycle = 0;

  while ((c = getopt(argc, argv, "hbdlr:w8:")) != -1) {
    switch (c) {
    case 'h':
      display_help(argv[0]);
      return EXIT_SUCCESS;

    case 'b':
      debugger_break = true;
      break;

    case 'd':
      dormann_test = true;
      break;

    case 'l':
      lorenz_test = true;
      break;

    case 'r':
      rom_directory = optarg;
      break;

    case 'w':
      warp_mode = true;
      break;

    case '8':
      d64_filename = optarg;
      break;

    case '?':
    default:
      display_help(argv[0]);
      return EXIT_FAILURE;
    }
  }

  mos6510_trace_init();
  debugger_stack_trace_init();
  panic_msg[0] = '\0';

  mem_init(&mem);
  cia_init(&cia1, 1);
  cia_init(&cia2, 2);
  vic_init(&vic);
  serial_bus_init(&bus);
  disk_init();

  /* Lorenz test mode: */
  if (lorenz_test) {
    lorenz_test_setup(&cpu, &mem);
    while (1) {
      mos6510_trace_add(&cpu, &mem);
      mos6510_execute(&cpu, &mem);
    }
    return EXIT_SUCCESS;

  /* Dormann test mode: */
  } else if (dormann_test) {
    dormann_test_setup(&cpu, &mem);
    while (1) {
      mos6510_trace_add(&cpu, &mem);
      mos6510_execute(&cpu, &mem);
    }
    return EXIT_SUCCESS;
  }

  /* Commodore 64 mode: */
  snprintf(rom_path, PATH_MAX, "%s/%s",
    (rom_directory) ? rom_directory : DEFAULT_ROM_DIRECTORY, KERNAL_ROM);
  if (mem_load_rom(&mem, rom_path, 0xE000) != 0) {
    fprintf(stdout, "Loading of ROM '%s' failed!\n", rom_path);
    return EXIT_FAILURE;
  }

  snprintf(rom_path, PATH_MAX, "%s/%s",
    (rom_directory) ? rom_directory : DEFAULT_ROM_DIRECTORY, BASIC_ROM);
  if (mem_load_rom(&mem, rom_path, 0xA000) != 0) {
    fprintf(stdout, "Loading of ROM '%s' failed!\n", rom_path);
    return EXIT_FAILURE;
  }

  snprintf(rom_path, PATH_MAX, "%s/%s",
    (rom_directory) ? rom_directory : DEFAULT_ROM_DIRECTORY, CHAR_ROM);
  if (mem_load_rom(&mem, rom_path, 0xD000) != 0) {
    fprintf(stdout, "Loading of ROM '%s' failed!\n", rom_path);
    return EXIT_FAILURE;
  }

  /* Setup CIA connections: */
  mem.cia_read = cia_read_hook;
  mem.cia_write = cia_write_hook;
  mem.cia1 = &cia1;
  mem.cia2 = &cia2;
  cia1.cpu = &cpu;
  cia1.mem = &mem;
  cia2.cpu = &cpu;
  cia2.mem = &mem;

  /* Setup VIC-II connections: */
  mem.vic_read = vic_read_hook;
  mem.vic_write = vic_write_hook;
  mem.vic = &vic;
  vic.cpu = &cpu;
  vic.mem = &mem;

#ifdef RESID
  /* Setup reSID. */
  mem.sid_read = resid_read_hook;
  mem.sid_write = resid_write_hook;
  if (resid_init() != 0) {
    return EXIT_FAILURE;
  }
#endif
  joystick_init();

  mos6510_reset(&cpu, &mem);
  console_init();
  signal(SIGINT, sig_handler);

  /* Autoload PRG if specified. */
  if (argc > optind) {
    pending_prg = argv[optind];
  }

  /* Load D64 file if specified. */
  if (d64_filename != NULL) {
    if (disk_load_d64(8, d64_filename) != 0) {
      console_exit();
      fprintf(stdout, "Loading of D64 file '%s' failed!\n", d64_filename);
      return EXIT_FAILURE;
    }
  }

  /* Setup timer to relax CPU. */
  struct itimerval new;
  new.it_value.tv_sec = 0;
  new.it_value.tv_usec = 10000;
  new.it_interval.tv_sec = 0;
  new.it_interval.tv_usec = 10000;
  signal(SIGALRM, sig_handler);
  setitimer(ITIMER_REAL, &new, NULL);

  while (1) {
    mos6510_trace_add(&cpu, &mem);
    mos6510_execute(&cpu, &mem);
    sync_cycle += cpu.cycles;
#ifdef RESID
    resid_execute(cpu.cycles, warp_mode);
#endif
    while (cpu.cycles > 0) { /* Run once for each CPU clock. */
      cia_execute(&cia1);
      cia_execute(&cia2);
      vic_execute(&vic);
      cpu.cycles--;
    }
    serial_bus_execute(&bus, &cia2.data_port_a);
#ifdef CONSOLE_EXTRA_INFO
    console_extra_info.pc = cpu.pc;
    console_extra_info.a = cpu.a;
    console_extra_info.x = cpu.x;
    console_extra_info.y = cpu.y;
    console_extra_info.sp = cpu.sp;
    console_extra_info.sr_n = cpu.sr.n;
    console_extra_info.sr_v = cpu.sr.v;
    console_extra_info.sr_b = cpu.sr.b;
    console_extra_info.sr_d = cpu.sr.d;
    console_extra_info.sr_i = cpu.sr.i;
    console_extra_info.sr_z = cpu.sr.z;
    console_extra_info.sr_c = cpu.sr.c;
#endif
    console_execute(&mem, &vic);
    joystick_execute();

    if (debugger_break) {
#ifdef RESID
      resid_pause();
#endif
      console_pause();
      if (panic_msg[0] != '\0') {
        fprintf(stdout, "%s", panic_msg);
        panic_msg[0] = '\0';
      }
      debugger_break = debugger(&cpu, &mem, &bus);
      if (! debugger_break) {
#ifdef RESID
        resid_resume();
#endif
        console_resume();
      }
    }

    if (pending_prg != NULL) {
      if (cpu.pc == 0xE5D4) { /* KERNAL should now be ready for commands. */
        if (mem_load_prg(&mem, pending_prg) != 0) {
          console_exit();
          fprintf(stdout, "Loading of PRG '%s' failed!\n", pending_prg);
          return EXIT_FAILURE;
        }

        /* Inject a RUN command. */
        mem.ram[0x277] = 'R';
        mem.ram[0x278] = 'U';
        mem.ram[0x279] = 'N';
        mem.ram[0x27A] = '\r';
        mem.ram[0x27B] = '\r';
        mem.ram[0xC6] = 5;

        pending_prg = NULL;
      }
    }

    if (! warp_mode) {
#ifdef RESID
      if (sync_cycle > resid_sync()) { /* Let reSID control the speed. */
#else
      if (sync_cycle > 9852) { /* Tuned to approximately PAL C64 speed. */
#endif
        sync_cycle = 0;
        pause(); /* Wait for SIGALRM. */
      }
    }
  }

  return EXIT_SUCCESS;
}



