#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef UNICODE
#define NCURSES_WIDECHAR 1
#include <ncursesw/ncurses.h>
#include <locale.h>
#else
#include <curses.h>
#endif
#include <unistd.h>
#include <sys/time.h>

#include "mem.h"
#include "cia.h"
#include "vic.h"
#include "panic.h"



/* Using colors from the default xterm/rxvt 256 color palette. */
static int color_map[16] = {
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
 240, /* 11 = Grey 1      */
 244, /* 12 = Grey 2      */
 156, /* 13 = Light Green */
  63, /* 14 = Light Blue  */
 250, /* 15 = Grey 3      */
};



#ifdef UNICODE
static wchar_t charset1_to_unicode[128] = {
  '@',    'A',    'B',    'C',    'D',    'E',    'F',    'G',
  'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
  'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
  'X',    'Y',    'Z',    '[',    0x00A3, ']',    0x2191, 0x2190,
  ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '\'',
  '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
  '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
  '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
  0x2501, 0x2660, 0x2503, 0x2501, 0x1FB77,0x1FB76,0x1FB7A,0x1FB71,
  0x1FB74,0x256E, 0x2570, 0x256F, 0x1FB7C,0x2572, 0x2571, 0x1FB7D,
  0x1FB7E,0x2022, 0x1FB7B,0x2665, 0x1FB70,0x256D, 0x2573, 0x25CB,
  0x2663, 0x1FB75,0x2666, 0x254B, 0x1FB8C,0x2503, 0x03C0, 0x25E5,
  ' ',    0x258C, 0x2584, 0x2594, 0x2581, 0x258E, 0x1FB90,0x1FB87,
  0x1FB8F,0x25E4, 0x1FB87,0x2523, 0x2597, 0x2517, 0x2513, 0x2582,
  0x250F, 0x253B, 0x2533, 0x252B, 0x258E, 0x258D, 0x1FB88,0x1FB82,
  0x1FB83,0x2583, 0x1FB7F,0x2596, 0x259D, 0x251B, 0x2598, 0x259A,
};

static wchar_t charset2_to_unicode[128] = {
  '@',    'a',    'b',    'c',    'd',    'e',    'f',    'g',
  'h',    'i',    'j',    'k',    'l',    'm',    'n',    'o',
  'p',    'q',    'r',    's',    't',    'u',    'v',    'w',
  'x',    'y',    'z',    '[',    0x00A3, ']',    0x2191, 0x2190,
  ' ',    '!',    '"',    '#',    '$',    '%',    '&',    '\'',
  '(',    ')',    '*',    '+',    ',',    '-',    '.',    '/',
  '0',    '1',    '2',    '3',    '4',    '5',    '6',    '7',
  '8',    '9',    ':',    ';',    '<',    '=',    '>',    '?',
  0x2501, 'A',    'B',    'C',    'D',    'E',    'F',    'G',
  'H',    'I',    'J',    'K',    'L',    'M',    'N',    'O',
  'P',    'Q',    'R',    'S',    'T',    'U',    'V',    'W',
  'X',    'Y',    'Z',    0x254B, 0x1FB8C,0x2503, 0x1FB90,0x1FB98,
  ' ',    0x258C, 0x2584, 0x2594, 0x2581, 0x258E, 0x1FB90,0x1FB87,
  0x1FB8F,0x1FB99,0x1FB87,0x2523, 0x2597, 0x2517, 0x2513, 0x2582,
  0x250F, 0x253B, 0x2533, 0x252B, 0x258E, 0x258D, 0x1FB88,0x1FB82,
  0x1FB83,0x2583, 0x2713, 0x2596, 0x259D, 0x251B, 0x2598, 0x259A,
};
#else
static const uint8_t charset1_to_ascii[128] = {
  '@','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','[','&',']','^','<',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '-','s','|','-','-','-','-','|','|','\\','\\','/','\\','\\','/','/',
  '\\','*','-','h','|','/','x','*','c','|','d','+','#','|','p','\\',
  ' ','#','#','-','_','|','#','|','#','/','|','|','#','\\','\\','_',
  '/','-','-','|','|','|','|','-','-','-','/','#','#','/','#','#',
};

static const uint8_t charset2_to_ascii[128] = {
  '@','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
  'p','q','r','s','t','u','v','w','x','y','z','[','&',']','^','<',
  ' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
  '0','1','2','3','4','5','6','7','8','9',':',';','<','=','>','?',
  '.','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',
  'P','Q','R','S','T','U','V','W','X','Y','Z','+','#','|','#','#',
  ' ','#','#','-','_','|','#','|','#','/','|','|','#','\\','\\','_',
  '/','-','-','|','|','|','|','-','-','-','/','#','#','/','#','#',
};
#endif /* UNICODE */



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
#ifdef UNICODE
  setlocale(LC_ALL, "en_US.UTF-8");
#ifdef C64_TTF
  for (int i = 0; i < 128; i++) {
    /* Re-map to private Unicode space. */
    charset1_to_unicode[i] = 0xEE00 + i;
    charset2_to_unicode[i] = 0xEF00 + i;
  }
#endif /* C64_TTF */
#endif /* UNICODE */
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



void console_execute(mem_t *mem, vic_t *vic)
{
  static int cycle = 0;
  int row, col;
  uint16_t address;
  uint8_t petscii;
#ifdef UNICODE
  attr_t attr;
  cchar_t cchar;
#endif /* UNICODE */
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
      address += ((vic->mp >> 4) & 0xF) * 0x400;
      /* ...Finally add the column and row. */
      address += col;
      address += (row * 40);
      reverse = mem->ram[address] >> 7;
      charset = (vic->mp >> 1) & 1;

#ifdef UNICODE
      if (has_colors() && can_change_color()) {
        color_pair = (vic->bg0 * 16) + 
          (vic->color_ram[col + (row * 40)] & 0xF) + 1;
      } else {
        color_pair = 0;
      }

      if (reverse) {
        attr = A_REVERSE;
      } else {
        attr = 0;
      }

      if (charset) {
        setcchar(&cchar, &charset2_to_unicode[mem->ram[address] & 0x7F],
          attr, color_pair, NULL);
      } else {
        setcchar(&cchar, &charset1_to_unicode[mem->ram[address] & 0x7F],
          attr, color_pair, NULL);
      }
      mvadd_wch(row, col, &cchar);
#else
      if (has_colors() && can_change_color()) {
        color_pair = (vic->bg0 * 16) + 
          (vic->color_ram[col + (row * 40)] & 0xF) + 1;
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
#endif /* UNICODE */
    }
  }
#ifdef CONSOLE_EXTRA_INFO
  /* CPU */
  attron(A_BOLD);
  mvprintw(26,  1, "/// 6510 ///");
  attroff(A_BOLD);
  mvprintw(27,  1, "PC:%04x", console_extra_info.pc);
  mvprintw(27, 10, "SR:");
  mvprintw(27, 13, "%c", (console_extra_info.sr_n) ? 'N' : '.');
  mvprintw(27, 14, "%c", (console_extra_info.sr_v) ? 'V' : '.');
  mvprintw(27, 15, "-");
  mvprintw(27, 16, "%c", (console_extra_info.sr_b) ? 'B' : '.');
  mvprintw(27, 17, "%c", (console_extra_info.sr_d) ? 'D' : '.');
  mvprintw(27, 18, "%c", (console_extra_info.sr_i) ? 'I' : '.');
  mvprintw(27, 19, "%c", (console_extra_info.sr_z) ? 'Z' : '.');
  mvprintw(27, 20, "%c", (console_extra_info.sr_c) ? 'C' : '.');
  mvprintw(28,  1, "A:%02x", console_extra_info.a);
  mvprintw(28,  7, "X:%02x", console_extra_info.x);
  mvprintw(28, 13, "Y:%02X", console_extra_info.y);
  mvprintw(28, 19, "SP:%02x", console_extra_info.sp);

  /* SID */
  attron(A_BOLD);
  mvprintw(30, 1, "/// 6581 ///");
  attroff(A_BOLD);
  for (int i = 0; i < 32; i += 8) {
    mvprintw(31 + (i / 8), 1, "$D4%02x", i);
    for (int j = 0; j < 8; j++) {
      mvprintw(31 + (i / 8), 8 + (j * 3), "%02x",
        console_extra_info.sid[i + j]);
    }
  }
#endif
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
      } else {
        petscii = 0; /* Unhandled input code */
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



