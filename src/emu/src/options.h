#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include "pdp8/emulator.h"

extern char **get_option_names();
extern int enable_option(char *name, pdp8_t *pdp8);

#endif

