#ifndef _TTY_DRIVER_H_
#define _TTY_DRIVER_H_

#include "pdp8/emulator.h"

typedef struct tty_driver_t tty_driver_t;

extern tty_driver_t *emu_install_tty(pdp8_t *pdp8);
extern void emu_start_tty(tty_driver_t *tty);   
extern void emu_end_tty(tty_driver_t *tty);    
#endif

