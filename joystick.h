#ifndef _JOYSTICK_H
#define _JOYSTICK_H

#include <stdint.h>

int joystick_init(void);
void joystick_execute(void);
uint8_t joystick_port_1_get(void);
uint8_t joystick_port_2_get(void);

#endif /* _JOYSTICK_H */
