#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <stdint.h>

typedef void (*scheduler_event_t)(void *ctx);
typedef struct scheduler_t scheduler_t;

typedef struct scheduler_callback_t {
    uint64_t time;
    scheduler_event_t event;
    void *ctx;
} scheduler_callback_t;

extern scheduler_t *scheduler_create();
extern void scheduler_free(scheduler_t *sch);
extern int scheduler_insert(scheduler_t *sch, uint64_t time, scheduler_event_t event, void *ctx);
extern int scheduler_delete(scheduler_t *sch, scheduler_event_t event, void *ctx);
extern void scheduler_clear(scheduler_t *sch);
extern int scheduler_empty(scheduler_t *sch);
extern uint64_t scheduler_next_event_time(scheduler_t *sch);
extern int scheduler_extract_min(scheduler_t *sch, scheduler_callback_t *callback);

extern void scheduler_print(scheduler_t *sch);

#endif
