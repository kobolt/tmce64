#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "mem.h"

void console_pause(void);
void console_resume(void);
void console_exit(void);
void console_init(void);
void console_execute(mem_t *mem);

#endif /* _CONSOLE_H */
