#ifndef _PDP8_TRACE_H_
#define _PDP8_TRACE_H_

#include "pdp8/emulator.h"

typedef struct pdp8_trace_t pdp8_trace_t;

#define TRACE_UNLIMITED 0
extern pdp8_trace_t *pdp8_trace_create(struct pdp8_t *pdp8, uint32_t buffer_size);
extern void pdp8_trace_free(pdp8_trace_t *trc);
extern void pdp8_trace_begin_instruction(pdp8_trace_t *trc);
extern void pdp8_trace_end_instruction(pdp8_trace_t *trc);
extern void pdp8_trace_interrupt(pdp8_trace_t *trc);
extern void pdp8_trace_memory_write(pdp8_trace_t *trc, uint16_t addr, uint12_t data);
extern int pdp8_trace_save(pdp8_trace_t *trc, char *fn);
extern pdp8_trace_t *pdp8_trace_load(char *fn);
extern int pdp8_trace_save_listing(pdp8_trace_t *trc, char *fn);

#endif
