#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer/ring_buffer.h"

struct ring_buffer_t {
    rb_ptr_t head;
    rb_ptr_t tail;
    rb_ptr_t size;
    uint8_t *buf;
    rb_delete_t del_notify;
    void *ctx_notify;
    rb_ptr_t wrstart;             /* start and end of writable region */
    rb_ptr_t wrend;               /* in last allocated event */
    rb_ptr_t rdstart;
    rb_ptr_t rdend;
};

#define EMPTY(rb) ((rb)->head == (rb)->tail)
#define INC(rb, p, n) (((p) + (n)) % ((rb)->size))
#define NEXT(rb, p) INC(rb, p, 1)

/* NOTE that DEC is only valid for 0 <= n <= buffer-size */
#define DEC(rb, p, n) (((p) - (n) + (rb)->size) % ((rb)->size))
#define PREV(rb, p) DEC(rb, p, 1)

/* tests if `p` is in the open range [s..e) */
#define BETWEEN(s, e, p) ((s) <= (e) ? (s) <= (p) && (p) < (e) : (s) <= (p) || (p) < (e))

static const uint32_t sig = 0x52423031;           /* RB01 */

ring_buffer_t *rb_create(size_t bytes, rb_delete_t del_notify, void *ctx) {
    ring_buffer_t *rb = calloc(1, sizeof(ring_buffer_t));
    if (rb) {
        rb->buf = calloc(1, bytes);
        if (!rb->buf) {
            free(rb);
            rb = NULL;
        }
    }

    if (rb) {
        rb->size = bytes;
        rb->del_notify = del_notify;
        rb->ctx_notify = ctx;
    }

    return rb;
}

void rb_destroy(ring_buffer_t *rb) {
    if (rb) {
        free(rb->buf);
        free(rb);
    }
}

#define HDR_SIG (0)
#define HDR_HEAD (1)
#define HDR_TAIL (2)
#define HDR_SIZE (3)
#define HDR_CNT (4)

int rb_save(ring_buffer_t *rb, FILE *fp) {
    uint32_t hdr[HDR_CNT];
    hdr[HDR_SIG] = htonl(sig);
    hdr[HDR_HEAD] = htonl(rb->head);
    hdr[HDR_TAIL] = htonl(rb->tail);
    hdr[HDR_SIZE] = htonl(rb->size);

    if (fwrite(hdr, sizeof(uint32_t), HDR_CNT, fp) != HDR_CNT) {
        return -1;
    }

    if (fwrite(rb->buf, 1, rb->size, fp) != rb->size) {
        return -1;
    }

    return 0;
}

ring_buffer_t *rb_load(FILE *fp) {
    uint32_t hdr[HDR_CNT];

    if (fread(hdr, sizeof(uint32_t), HDR_CNT, fp) != HDR_CNT) {
        return NULL;
    }

    for (int i = 0; i < HDR_CNT; i++) {
        hdr[i] = ntohl(hdr[i]);
    }

    if (hdr[HDR_SIG] != sig) {
        errno = EINVAL;
        return NULL;
    }

    ring_buffer_t *rb = calloc(1, sizeof(ring_buffer_t));
    if (!rb) {
        return NULL;
    }

    rb->head = hdr[HDR_HEAD];
    rb->tail = hdr[HDR_TAIL];
    rb->size = hdr[HDR_SIZE];

    rb->buf = malloc(rb->size);
    if (!rb->buf) {
        free(rb);
        return NULL;
    }

    if (fread(rb->buf, 1, rb->size, fp) != rb->size) {
        rb_destroy(rb);
        rb = NULL;
    }

    return rb;
}


rb_ptr_t rb_alloc_event(ring_buffer_t *rb, uint8_t type, uint8_t bytes) {
    /* NB bytes doesn't include the leading type and byte count fields.
     * Also, we always leave 1 byte free so the condition head == tail
     * is unambiguous -- it means the buffer is empty.
     */
    if (bytes + 3 >= rb->size) {
        return RB_NULL;
    }

    rb_ptr_t new_head = (rb->head + 2 + bytes) % rb->size;

    while (rb->head != rb->tail && (BETWEEN(rb->head, new_head, rb->tail) || new_head == rb->tail)) {
        rb_ptr_t type = rb->buf[rb->tail];
        rb->tail = NEXT(rb, rb->tail);
        rb_ptr_t cnt = rb->buf[rb->tail];
        rb->tail = NEXT(rb, rb->tail);
        if (rb->del_notify) {
            rb->rdstart = rb->tail;
            rb->rdend = INC(rb, rb->rdstart, cnt);
            rb->del_notify(rb->ctx_notify, type, cnt, rb->tail);
        }
        rb->tail = INC(rb, rb->tail, cnt);
    }

    rb->buf[rb->head] = type;
    rb->head = NEXT(rb, rb->head);
    rb->buf[rb->head] = bytes;
    rb->head = NEXT(rb, rb->head);
    rb_ptr_t dp = rb->head;
    rb->head = INC(rb, rb->head, bytes);

    rb->wrstart = dp;
    rb->wrend = rb->head;

    return dp;
}

rb_ptr_t rb_put_uint8(ring_buffer_t *rb, rb_ptr_t p, uint8_t value) {
    rb_ptr_t pend = NEXT(rb, p);
    if (p == RB_NULL ||
        !BETWEEN(rb->wrstart, rb->wrend, p) ||
        (pend != rb->wrend && !BETWEEN(rb->wrstart, rb->wrend, pend))) {
        return RB_NULL;
    }

    rb->buf[p] = value;
    return pend;
}

rb_ptr_t rb_put_uint16(ring_buffer_t *rb, rb_ptr_t p, uint16_t value) {
    rb_ptr_t pend = INC(rb, p, sizeof(uint16_t));
    if (p == RB_NULL ||
        !BETWEEN(rb->wrstart, rb->wrend, p) ||
        (pend != rb->wrend && !BETWEEN(rb->wrstart, rb->wrend, pend))) {
        return RB_NULL;
    }

    *(uint16_t *)(rb->buf + p) = value;
    return pend;
}

rb_ptr_t rb_put_uint32(ring_buffer_t *rb, rb_ptr_t p, uint32_t value) {
    rb_ptr_t pend = INC(rb, p, sizeof(uint32_t));
    if (p == RB_NULL ||
        !BETWEEN(rb->wrstart, rb->wrend, p) ||
        (pend != rb->wrend && !BETWEEN(rb->wrstart, rb->wrend, pend))) {
        return RB_NULL;
    }

    *(uint32_t *)(rb->buf + p) = value;
    return pend;
}

rb_ptr_t rb_first_event(ring_buffer_t *rb, uint8_t *type, uint8_t *bytes) {
    if (rb->head == rb->tail) {
        return RB_NULL;
    }

    rb_ptr_t p = rb->tail;

    *type = rb->buf[p];
    p = NEXT(rb, p);
    *bytes = rb->buf[p];
    p = NEXT(rb, p);

    rb_ptr_t dp = p;
    p = INC(rb, p, *bytes);

    rb->rdstart = dp;
    rb->rdend = p;

    return dp;
}

rb_ptr_t rb_next_event(ring_buffer_t *rb, rb_ptr_t p, uint8_t *type, uint8_t *bytes) {
    /* p is the data pointer of an event; the count immediately preceeds it
     */
    uint8_t cnt = rb->buf[PREV(rb, p)];
    p = INC(rb, p, cnt);

    if (p == rb->head) {
        return RB_NULL;
    }

    *type = rb->buf[p];
    p = NEXT(rb, p);
    *bytes = rb->buf[p];
    p = NEXT(rb, p);

    rb_ptr_t dp = p;
    p = INC(rb, p, *bytes);

    rb->rdstart = dp;
    rb->rdend = p;

    return dp;
}

rb_ptr_t rb_get_uint8(ring_buffer_t *rb, rb_ptr_t p, uint8_t *value) {
    rb_ptr_t pend = NEXT(rb, p);
    if (p == RB_NULL ||
        !BETWEEN(rb->rdstart, rb->rdend, p) ||
        (pend != rb->rdend && !BETWEEN(rb->rdstart, rb->rdend, pend))) {
        return RB_NULL;
    }

    *value = rb->buf[p];
    return pend;
}

rb_ptr_t rb_get_uint16(ring_buffer_t *rb, rb_ptr_t p, uint16_t *value) {
    rb_ptr_t pend = INC(rb, p, sizeof(uint16_t));
    if (p == RB_NULL ||
        !BETWEEN(rb->rdstart, rb->rdend, p) ||
        (pend != rb->rdend && !BETWEEN(rb->rdstart, rb->rdend, pend))) {
        return RB_NULL;
    }

    *value = *(uint16_t *)(rb->buf + p);
    return pend;
}

rb_ptr_t rb_get_uint32(ring_buffer_t *rb, rb_ptr_t p, uint32_t *value) {
    rb_ptr_t pend = INC(rb, p, sizeof(uint32_t));
    if (p == RB_NULL ||
        !BETWEEN(rb->rdstart, rb->rdend, p) ||
        (pend != rb->wrend && !BETWEEN(rb->rdstart, rb->rdend, pend))) {
        return RB_NULL;
    }

    *value = *(uint32_t *)(rb->buf + p);
    return pend;
}
