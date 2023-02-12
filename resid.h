#ifndef _RESID_H
#define _RESID_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t resid_read_hook(void *dummy, uint16_t address);
void resid_write_hook(void *dummy, uint16_t address, uint8_t value);
void resid_pause(void);
void resid_resume(void);
int resid_init(void);
void resid_execute(uint32_t cycles, bool warp_mode_active);
int32_t resid_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* _RESID_H */
