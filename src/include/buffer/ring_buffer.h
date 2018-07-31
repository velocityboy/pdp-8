/*
 * A circular memory buffer that contains variable size events.
 * Events contain a leading type byte and length byte, and can
 * wrap around the end to the start of the buffer.
 */
#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

typedef struct ring_buffer_t ring_buffer_t;

typedef int rb_ptr_t;

#define RB_NULL ((rb_ptr_t)-1)

typedef void (*rb_delete_t)(void *ctx, uint8_t type, uint8_t len, rb_ptr_t start);

extern ring_buffer_t *rb_create(size_t bytes, rb_delete_t del_notify, void *ctx, int opts);
#define RB_OPT_ALLOC_CAN_FAIL      (0000)  /* alloc will fail if queue is full */
#define RB_OPT_ALLOC_MAKES_SPACE   (0001)  /* alloc will make space if queue is full */

extern void rb_destroy(ring_buffer_t *rb);
extern int rb_save(ring_buffer_t *rb, FILE *fp);
extern ring_buffer_t *rb_load(FILE *fp);
extern ring_buffer_t *rb_load_from_memory(uint8_t *p, size_t *len);

extern int rb_max_payload_bytes(ring_buffer_t *rb);

/* Return the start of the event, or a RB_NULL if the buffer isn't
 * big enough. If earlier events have to be dropped and a notify
 * callback was provided, it will be called for each deleted event.
 */
extern rb_ptr_t rb_alloc_event(ring_buffer_t *rb, uint8_t type, uint8_t bytes);

/* Stores the given data type and returns an advanced pointer.
 * Returns RB_NULL if the data would be outside the bounds of the
 * last allocated event.
 */
extern rb_ptr_t rb_put_uint8(ring_buffer_t *rb, rb_ptr_t p, uint8_t value);
extern rb_ptr_t rb_put_uint16(ring_buffer_t *rb, rb_ptr_t p, uint16_t value);
extern rb_ptr_t rb_put_uint32(ring_buffer_t *rb, rb_ptr_t p, uint32_t value);

/* Get the earliest event in the buffer, or the next event in the buffer.
 * Returns an rb_ptr_t to the event's data or RB_NULL if there is no next event.
 *
 * Note that as there is no write locking, interleaving reads and writes
 * is dangerous, and it is up to the caller to guarantee safety.
 */
extern rb_ptr_t rb_first_index(ring_buffer_t *rb);
extern rb_ptr_t rb_first_event(ring_buffer_t *rb, uint8_t *type, uint8_t *bytes);
extern rb_ptr_t rb_next_event(ring_buffer_t *rb, rb_ptr_t cur_event, uint8_t *type, uint8_t *bytes);

extern void rb_remove_first_event(ring_buffer_t *rb);

/* Gets the given data type at `p' and returns an advanced pointer.
 * Returns RB_NULL if the data would not come from the bounds of the last
 * event from rb_first_event or rb_next_event.
 */
extern rb_ptr_t rb_get_uint8(ring_buffer_t *rb, rb_ptr_t p, uint8_t *value);
extern rb_ptr_t rb_get_uint16(ring_buffer_t *rb, rb_ptr_t p, uint16_t *value);
extern rb_ptr_t rb_get_uint32(ring_buffer_t *rb, rb_ptr_t p, uint32_t *value);

#endif
