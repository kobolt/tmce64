#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define JOYSTICK_MAX 4


static SDL_Joystick *joystick[JOYSTICK_MAX] = {NULL,NULL,NULL,NULL};
static int joysticks_connected = 0;

static uint8_t joystick_port_1 = 0xFF;
static uint8_t joystick_port_2 = 0xFF;



static void joystick_exit(void)
{
  int i;

  for (i = 0; i < joysticks_connected; i++) {
    if (SDL_JoystickGetAttached(joystick[i])) {
      SDL_JoystickClose(joystick[i]);
    }
  }
}



int joystick_init(void)
{
  int i;

#ifndef RESID
  /* Skip if SDL was already initalized by resid module. */
  if (SDL_Init(SDL_INIT_JOYSTICK) != 0) {
    fprintf(stderr, "Unable to initalize SDL: %s\n", SDL_GetError());
    return -1;
  }
#endif
  atexit(joystick_exit);

  /* Just open all the joysticks. */
  for (i = 0; i < SDL_NumJoysticks(); i++) {
    joystick[i] = SDL_JoystickOpen(i);
    joysticks_connected++;
    if (joysticks_connected >= JOYSTICK_MAX) {
      break;
    }
  }

  return 0;
}



void joystick_execute(void)
{
  static int cycle = 0;
  SDL_Event event;

  /* Only run every X cycle. */
  cycle++;
  if (cycle % 10000 != 0) {
    return;
  }

  while (SDL_PollEvent(&event) == 1) {
    switch (event.type) {
    case SDL_QUIT:
      exit(EXIT_SUCCESS);
      break;

    case SDL_JOYAXISMOTION:
      if (event.jaxis.axis == 0) {
        if (event.jaxis.value > 16384) { /* Right Pressed */
          joystick_port_2 &= ~0x8;
          joystick_port_2 |=  0x4;
        } else if (event.jaxis.value < -16384) { /* Left Pressed */
          joystick_port_2 |=  0x8;
          joystick_port_2 &= ~0x4;
        } else { /* Right/Left Released */
          joystick_port_2 |= 0xC;
        }

      } else if (event.jaxis.axis == 1) {
        if (event.jaxis.value > 16384) { /* Down Pressed */
          joystick_port_2 &= ~0x2;
          joystick_port_2 |=  0x1;
        } else if (event.jaxis.value < -16384) { /* Up Pressed */
          joystick_port_2 |=  0x2;
          joystick_port_2 &= ~0x1;
        } else { /* Down/Up Released */
          joystick_port_2 |= 0x3;
        }
      }
      break;

    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
      switch (event.jbutton.button) {
      case 0:
      case 1:
      case 2:
      case 3:
        if (event.jbutton.state == 1) {
          joystick_port_2 &= ~0x10;
        } else {
          joystick_port_2 |=  0x10;
        }
        break;
      }
      break;
    }
  }
}



uint8_t joystick_port_1_get(void)
{
  return joystick_port_1;
}



uint8_t joystick_port_2_get(void)
{
  return joystick_port_2;
}



