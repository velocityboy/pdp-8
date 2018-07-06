#ifndef _PDP8_EMULATOR_H_
#define _PDP8_EMULATOR_H_

#include <stdint.h>

#include "pdp8/defines.h"

typedef uint16_t uint12_t;
typedef struct pdp8_t pdp8_t;

#define PDP8_DEVICE_IDS 0100


typedef void (*device_handler_t)(uint12_t op, pdp8_t *pdp8);

/*
 * PDP8 emulator state 
 */
struct pdp8_t {
    uint12_t *core;
    int core_size;
    uint12_t ac;
    unsigned link : 1;
    unsigned run : 1;
    unsigned option_eae : 1;        /* Type 182 extended arithmetic element (i.e. mul and div) */
    uint12_t pc;
    uint12_t sr;        /* front panel switches */

    /* these registers are only used if option_eae is installed */
    uint12_t mq;
    uint12_t sc;

    device_handler_t device_handlers[PDP8_DEVICE_IDS];
};

extern pdp8_t *pdp8_create();
extern void pdp8_free(pdp8_t *pdp8);
extern void pdp8_clear(pdp8_t *pdp8);
extern void pdp8_step(pdp8_t *pdp8);

/* utilities */
extern int pdp8_disassemble(uint16_t addr, uint12_t op, char *decoded, int decodedLen);
extern pdp8_reg_t pdp8_reg_from_string(char *s);

#endif
