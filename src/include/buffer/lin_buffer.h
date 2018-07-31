/*
 * A linear memory buffer that contains variable size events,
 * which grows as new events are added.
 * Events contain a leading type byte and length byte.
 */
#ifndef _LIN_BUFFER_H_
#define _LIN_BUFFER_H_

typedef struct lin_buffer_t lin_buffer_t;

typedef int lb_ptr_t;

#define LB_NULL ((lb_ptr_t)-1)

extern lin_buffer_t *lb_create(size_t ini_bytes);
extern void lb_destroy(lin_buffer_t *lb);
extern int lb_save(lin_buffer_t *lb, FILE *fp);
extern lin_buffer_t *lb_load(FILE *fp);
extern lin_buffer_t *lb_load_from_memory(uint8_t *p, size_t *len);

/* Return the start of the event, or an LB_NULL on out of memory.
 */
extern lb_ptr_t lb_alloc_event(lin_buffer_t *lb, uint8_t type, uint8_t bytes);

/* Stores the given data type and returns an advanced pointer.
 * Returns LB_NULL if the data would be outside the bounds of the
 * last allocated event.
 */
extern lb_ptr_t lb_put_uint8(lin_buffer_t *lb, lb_ptr_t p, uint8_t value);
extern lb_ptr_t lb_put_uint16(lin_buffer_t *lb, lb_ptr_t p, uint16_t value);
extern lb_ptr_t lb_put_uint32(lin_buffer_t *lb, lb_ptr_t p, uint32_t value);

/* Get the earliest event in the buffer, or the next event in the buffer.
 * Returns an lb_ptr_t to the event's data or LB_NULL if there is no next event.
 */
extern lb_ptr_t lb_first_index(lin_buffer_t *lb);
extern lb_ptr_t lb_first_event(lin_buffer_t *lb, uint8_t *type, uint8_t *bytes);
extern lb_ptr_t lb_next_event(lin_buffer_t *lb, lb_ptr_t cur_event, uint8_t *type, uint8_t *bytes);

/* Gets the given data type at `p' and returns an advanced pointer.
 * Returns LB_NULL if the data would not come from the bounds of the last
 * event from lb_first_event or lb_next_event.
 */
extern lb_ptr_t lb_get_uint8(lin_buffer_t *lb, lb_ptr_t p, uint8_t *value);
extern lb_ptr_t lb_get_uint16(lin_buffer_t *lb, lb_ptr_t p, uint16_t *value);
extern lb_ptr_t lb_get_uint32(lin_buffer_t *lb, lb_ptr_t p, uint32_t *value);

#endif
