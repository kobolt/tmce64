#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>

#include <termios.h>
#include <unistd.h>

#include "cpu.h"
#include "mem.h"
#include "panic.h"
#include "test.h"



#define KERNAL_ROM "/usr/lib64/vice/C64/kernal"
#define BASIC_ROM "/usr/lib64/vice/C64/basic"
#define CHAR_ROM "/usr/lib64/vice/C64/chargen"



static uint8_t screencode_to_ascii[UINT8_MAX + 1] = {
  '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','[','&',']','.','.',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '.','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','[','&',']','.','.',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '.','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
  '.','.','.','.','.','.','.','.','.','.','.','.','.','.','.','.',
};



static uint8_t key_to_petscii[UINT8_MAX + 1] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,'\r', 0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  ' ','!','"','#','$','%','&',  0,'(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','[',  0,']',  0,  0,
    0,'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z',  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};



static cpu_t main_cpu;
static mem_t main_mem;

static bool print_ready = false;



static void crash_dump(void)
{ 
  fprintf(stderr, "Stack:\n");
  mem_ram_dump(stderr, &main_mem, MEM_PAGE_STACK, MEM_PAGE_STACK + 0xFF);
  
  fprintf(stderr, "Zero Page:\n");
  mem_ram_dump(stderr, &main_mem, 0, 0xFF);
  
  fprintf(stderr, "CPU Trace:\n");
  cpu_trace_dump(stderr);
}



static void sig_handler(int sig)
{
  fprintf(stderr, "Aborted!\n");
  crash_dump();
  exit(1);
}



static void exit_handler(void)
{
  /* Restore canonical mode and echo. */
  struct termios ts;
  tcgetattr(STDIN_FILENO, &ts);
  ts.c_lflag |= ICANON | ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &ts);
}



void panic(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  crash_dump();
  exit(1);
}



bool ram_read(mem_t *mem, uint16_t address, uint8_t *value)
{
  static int dummy_reads = 2;
  uint8_t petscii;
  int c;

  /* $00C6 = Length of keyboard buffer */
  if (address == 0xC6) {
    if (! print_ready) {
      *value = 0;
      return true;
    }

    dummy_reads++;
    if (dummy_reads <= 3) {
      *value = 1;
      return true;
    }
    dummy_reads = 0;

    /* This is a blocking read so the CPU will not spin at 100% */
    c = fgetc(stdin);
    if (c == EOF) {
      exit(0);
    } else {
      petscii = key_to_petscii[c];
      if (petscii != 0) {
        /* $0277 = Keyboard buffer, first entry */
        mem->ram[0x277] = petscii;
        *value = 1;
        return true;
      } else {
        dummy_reads = 2;
      }
    }
  }
  return false;
}



bool ram_write(mem_t *mem, uint16_t address, uint8_t value)
{
  static uint8_t last_cursor_row = 0;

  /* $00D6 = Current cursor row */
  if (address == 0xD6) {
    if (value != last_cursor_row) {
      printf("\n");
    }
    last_cursor_row = value;
  }

  /* $0400-$07E8 = Screen memory area */
  if (address >= 0x400 && address <= 0x7E8) {
    /* Waiting for the star to appear to avoid printing lots of garbage. */
    if ((! print_ready) && (value == '*')) {
      print_ready = true;
      printf("    ");
    }
    if (print_ready) {
      printf("%c", screencode_to_ascii[value]);
    }
  }

  return false;
}



int main(int argc, char *argv[])
{
  cpu_trace_init();
  signal(SIGINT, sig_handler);
  atexit(exit_handler);

  mem_init(&main_mem);

  if (argc > 1 && (0 == strcmp("lorenz", argv[1]))) {
    lorenz_test_setup(&main_cpu, &main_mem);

  } else if (argc > 1 && (0 == strcmp("dormann", argv[1]))) {
    dormann_test_setup(&main_cpu, &main_mem);

  } else {
    mem_load_rom(&main_mem, KERNAL_ROM, 0xE000);
    mem_load_rom(&main_mem, BASIC_ROM, 0xA000);
    mem_load_rom(&main_mem, CHAR_ROM, 0xD000);
    cpu_reset(&main_cpu, &main_mem);
    mem_ram_read_hook_install(ram_read);
    mem_ram_write_hook_install(ram_write);

    /* Turn off canonical mode and echo. */
    struct termios ts;
    tcgetattr(STDIN_FILENO, &ts);
    ts.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &ts);
  }

  while (1) {
    cpu_trace_add(&main_cpu, &main_mem);
    cpu_execute(&main_cpu, &main_mem);
  }

  return 0;
}



