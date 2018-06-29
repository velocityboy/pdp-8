#ifndef _DISLIB_H_
#define _DISLIB_H_

#include <stdint.h>

extern int pdp8Disassemble(uint16_t addr, uint16_t op, char *decoded, int decodedLen);

#endif
