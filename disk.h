#ifndef _DISK_H
#define _DISK_H

#include <stdint.h>
#include <stdio.h>

void disk_init(void);
int disk_load_d64(uint8_t device_no, const char *filename);
void disk_dump(FILE *fh);

#endif /* _DISK_H */
