#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <curses.h>
#include <unistd.h>
#include <sys/time.h>

#include "mem.h"
#include "cia.h"
#include "panic.h"

/* Using colors from the default rxvt color palette. */
int color_map[16] = {
   0, /*  0 = Black       */
  15, /*  1 = White       */
 124, /*  2 = Red         */
  80, /*  3 = Cyan        */
 129, /*  4 = Violet      */
  71, /*  5 = Green       */
  18, /*  6 = Blue        */
 228, /*  7 = Yellow      */
 172, /*  8 = Orange      */
  94, /*  9 = Brown       */
 210, /* 10 = Light Red   */
 234, /* 11 = Grey 1      */
 240, /* 12 = Grey 2      */
 156, /* 13 = Light Green */
  63, /* 14 = Light Blue  */
 250, /* 15 = Grey 3      */
};



static uint8_t charset1_to_ascii[128] = {
  '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','[','&',']','^','<',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '-','s','|','-','-','-','-','|','|','\\','\\','/','\\','\\','/','/',
  '\\','*','-','h','|','/','x','*','c','|','d','+','#','|','p','\\',
  ' ','#','#','-','_','|','#','|','#','/','|','|','#','\\','\\','_',
  '/','-','-','|','|','|','|','-','-','-','/','#','#','/','#','#',
};

static uint8_t charset2_to_ascii[128] = {
  '@','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w','x','y','z','[','&',']','^','<',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '.','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','+','#','|','#','#',
  ' ','#','#','-','_','|','#','|','#','/','|','|','#','\\','\\','_',
  '/','-','-','|','|','|','|','-','-','-','/','#','#','/','#','#',
};



static uint8_t key_to_petscii[UINT8_MAX + 1] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,141,'\r', 0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,
  ' ','!','"','#','$','%','&',  0,'(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '@',193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,'[',  0,']',  0,164,
    0,'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z',  0, 95,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};



void console_pause(void)
{
  endwin();
  timeout(-1);
}



void console_resume(void)
{
  timeout(0);
  refresh();
}



void console_exit(void)
{
  curs_set(1);
  endwin();
}



void console_init(void)
{
  int fg, bg;

  initscr();
  atexit(console_exit);
  noecho();
  keypad(stdscr, TRUE);
  timeout(0); /* Non-blocking mode. */
  curs_set(0); /* Hide cursor. */

  if (has_colors() && can_change_color()) {
    start_color();
    use_default_colors();

    for (bg = 0; bg < 16; bg++) {
      for (fg = 0; fg < 16; fg++) {
        init_pair((bg * 16) + fg + 1, color_map[fg], color_map[bg]);
      }
    }
  }
}



void console_execute(mem_t *mem)
{
  static int cycle = 0;
  int row, col;
  uint16_t address;
  uint8_t petscii;
  uint8_t color_pair;
  bool reverse;
  bool charset;
  int c;

  /* Only run every X cycle. */
  cycle++;
  if (cycle % 20000 != 0) {
    return;
  }

  /* Output */
  for (row = 0; row < 25; row++) {
    for (col = 0; col < 40; col++) {
      /* Start with the VIC-II bank selection... */
      address = ((~(((cia_t *)mem->cia2)->data_port_a) & 0x3) * 0x4000);
      /* ...Then add the screen memory area offset... */
      address += ((mem->vic2_mp >> 4) & 0xF) * 0x400;
      /* ...Finally add the column and row. */
      address += col;
      address += (row * 40);
      reverse = mem->ram[address] >> 7;
      charset = (mem->vic2_mp >> 1) & 1;

      if (has_colors() && can_change_color()) {
        color_pair = (mem->vic2_bg0 * 16) + 
          (mem->color_ram[col + (row * 40)] & 0xF) + 1;
        attron(COLOR_PAIR(color_pair));
      }

      if (reverse) {
        attron(A_REVERSE);
      }

      if (charset) {
        mvaddch(row, col, charset2_to_ascii[mem->ram[address] & 0x7F]);
      } else {
        mvaddch(row, col, charset1_to_ascii[mem->ram[address] & 0x7F]);
      }

      if (reverse) {
        attroff(A_REVERSE);
      }

      if (has_colors() && can_change_color()) {
        attroff(COLOR_PAIR(color_pair));
      }
    }
  }
  /* $00D3 = Current cursor column. */
  /* $00D6 = Current cursor row. */
  move(mem->ram[0xD6], mem->ram[0xD3]);
  refresh();

  /* Input */
  c = getch();
  if (c != ERR) {
    switch (c) {
    case KEY_RESIZE:
      break;

    case KEY_ENTER:
      petscii = 0x0D;
      break;

    case KEY_UP:
      petscii = 0x91;
      break;

    case KEY_DOWN:
      petscii = 0x11;
      break;

    case KEY_RIGHT:
      petscii = 0x1D;
      break;

    case KEY_LEFT:
      petscii = 0x9D;
      break;

    case KEY_BACKSPACE:
      petscii = 0x14;
      break;

    case KEY_F(1):
      petscii = 0x85;
      break;

    case KEY_F(2):
      petscii = 0x89;
      break;

    case KEY_F(3):
      petscii = 0x86;
      break;

    case KEY_F(4):
      petscii = 0x8A;
      break;

    case KEY_F(5):
      petscii = 0x87;
      break;

    case KEY_F(6):
      petscii = 0x8B;
      break;

    case KEY_F(7):
      petscii = 0x88;
      break;

    case KEY_F(8):
      petscii = 0x8C;
      break;

    case KEY_F(9):
      petscii = 0x05; /* White */
      break;

    case KEY_F(10):
      petscii = 0x1C; /* Red */
      break;

    case KEY_F(11):
      petscii = 0x1E; /* Green */
      break;

    case KEY_F(12):
      petscii = 0x1F; /* Blue */
      break;

    case KEY_F(13):
      petscii = 0x81; /* Orange */
      break;

    case KEY_F(14):
      petscii = 0x90; /* Black */
      break;

    case KEY_F(15):
      petscii = 0x95; /* Brown */
      break;

    case KEY_F(16):
      petscii = 0x96; /* Light Red */
      break;

    case KEY_F(17):
      petscii = 0x97; /* Dark Grey */
      break;

    case KEY_F(18):
      petscii = 0x98; /* Medium Grey */
      break;

    case KEY_F(19):
      petscii = 0x99; /* Light Green */
      break;

    case KEY_F(20):
      petscii = 0x9A; /* Light Blue */
      break;

    case KEY_F(21):
      petscii = 0x9B; /* Light Grey */
      break;

    case KEY_F(22):
      petscii = 0x9C; /* Purple */
      break;

    case KEY_F(23):
      petscii = 0x9E; /* Yellow */
      break;

    case KEY_F(24):
      petscii = 0x9F; /* Cyan */
      break;

    case KEY_HOME:
      petscii = 0x13;
      break;

    case KEY_DC: /* Delete */
      petscii = 0x93;
      break;

    case KEY_IC: /* Insert */
      petscii = 0x94;
      break;

    case KEY_NPAGE:
      petscii = 0x0E; /* Select character set #2. */
      break;

    case KEY_PPAGE:
      petscii = 0x8E; /* Select character set #1. */
      break;

    case KEY_END:
      petscii = 0x83; /* Run */
      break;

    case KEY_SEND: /* Shift + End */
      petscii = 0x03; /* Stop */
      break;

    default:
      if (c <= UINT8_MAX) {
        petscii = key_to_petscii[c];
        if (petscii == 0) {
          panic("Unmapped input code: %d\n", c);
        }
      } else {
        panic("Unhandled input code: %d\n", c);
        petscii = 0;
      }
      break;
    }
    if (petscii != 0) {
      /* $0277 = Keyboard buffer, first entry. */
      mem->ram[0x277] = petscii;
      /* $00C6 = Length of keyboard buffer. */
      mem->ram[0xC6] = 1;

      /* $0091 = Stop key indicator. */
      if (petscii == 0x03) {
        mem->ram[0x91] = 0x7F; /* Stop key is pressed. */
      } else {
        mem->ram[0x91] = 0xFF; /* Stop key is not pressed. */
      }
    }
  }
}



