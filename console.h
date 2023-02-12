#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "mem.h"
#include "vic.h"

void console_pause(void);
void console_resume(void);
void console_exit(void);
void console_init(void);
void console_execute(mem_t *mem, vic_t *vic);

#endif /* _CONSOLE_H */
