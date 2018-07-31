#ifndef _PDP8_TRACE_H_
#define _PDP8_TRACE_H_

#include <stdio.h>

#include "pdp8/emulator.h"

typedef struct pdp8_trace_t pdp8_trace_t;

typedef enum pdp8_trace_type_t {
  trc_out_of_memory,
  trc_begin_instruction,
  trc_end_instruction,
  trc_interrupt,
  trc_memory_changed,
  trc_end_of_trace,
} pdp8_trace_type_t;


typedef struct pdp8_trace_begin_instruction_t {
  uint16_t pc;
  uint16_t opwords[2];
} pdp8_trace_begin_instruction_t;

typedef struct pdp8_trace_reg_values_t {
  uint12_t ac;
  unsigned link : 1;
  unsigned eae_mode_b : 1;
  unsigned gt : 1;
  uint12_t sr;
  uint12_t mq;
  uint12_t sc;
  uint16_t ifr;
  uint16_t dfr;
  uint16_t ibr;
} pdp8_trace_reg_values_t;

typedef struct pdp8_trace_memory_changed_t {
  uint16_t addr;
  uint16_t data;
} pdp8_trace_memory_changed_t;

typedef struct pdp8_trace_event_t {
  pdp8_trace_type_t type;
  union {
    pdp8_trace_begin_instruction_t begin_instruction;
    pdp8_trace_reg_values_t end_instruction;
    pdp8_trace_memory_changed_t memory_changed;
  };
} pdp8_trace_event_t;

#define TRACE_UNLIMITED 0
extern pdp8_trace_t *pdp8_trace_create(struct pdp8_t *pdp8, uint32_t buffer_size);
extern void pdp8_trace_free(pdp8_trace_t *trc);
extern void pdp8_trace_begin_instruction(pdp8_trace_t *trc);
extern void pdp8_trace_end_instruction(pdp8_trace_t *trc);
extern void pdp8_trace_interrupt(pdp8_trace_t *trc);
extern void pdp8_trace_memory_write(pdp8_trace_t *trc, uint16_t addr, uint12_t data);
extern int pdp8_trace_save(pdp8_trace_t *trc, char *fn);
extern pdp8_trace_t *pdp8_trace_load(char *fn);
extern pdp8_trace_t *pdp8_trace_load_from_memory(uint8_t *bytes, size_t length);
extern int pdp8_trace_save_listing(pdp8_trace_t *trc, FILE *fp);

/*
 * pdp8_trace_first_event_marker returns an opaque marker for the first event.
 * pdp8_trace_next_event decodes an evet at the given marker and returns the marker of
 * the next event.
 * Markers are always positive, and a return of <0 indicates an error.
 * Markers may be saved, with the caveat that if the trace has a max buffer size, they may
 * become invalid if new events are added.
 */
extern void pdp8_trace_initial_registers(pdp8_trace_t *trc, pdp8_trace_reg_values_t *regs);
extern int pdp8_trace_first_event_marker(pdp8_trace_t *trc);
extern int pdp8_trace_next_event(pdp8_trace_t *trc, int marker, pdp8_trace_event_t *ev);

#endif
