#include <stdint.h>
#include <stdio.h>
#include <string.h>     /* for memset() */

#include "../src/ring_buffer.h"
#include "tests.h"

#ifndef MIN
# define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

DECLARE_TEST(rb_empty, "returns RB_NULL if buffer is empty") {
    ring_buffer_t *rb = rb_create(16, NULL, NULL);

    uint8_t type = 42;
    uint8_t bytes = 78;
    rb_ptr_t ev = rb_first_event(rb, &type, &bytes);

    ASSERT_V(ev == RB_NULL, "rb_first_event returns RB_NULL if buffer is empty");
    ASSERT_V(type == 42, "rb_first_event doesn't change type if buffer is empty");
    ASSERT_V(bytes == 78, "rb_first_event doesn't change bytes if buffer is empty");
    rb_destroy(rb);
}

DECLARE_TEST(rb_valid_event, "valid event can be stored and retrieved") {
    ring_buffer_t *rb = rb_create(16, NULL, NULL);

    static const uint8_t TYPE = 42;
    static const uint8_t SIZE = sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t);
    rb_ptr_t ev = rb_alloc_event(rb, TYPE, SIZE);

    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x11);
    ASSERT_V(ev != RB_NULL, "put byte succeeded");

    ev = rb_put_uint16(rb, ev, 0x2233);
    ASSERT_V(ev != RB_NULL, "put 16-bit word succeeded");

    ev = rb_put_uint32(rb, ev, 0x44556677);
    ASSERT_V(ev != RB_NULL, "put 32-bit word succeeded");

    ev = rb_put_uint8(rb, ev, 0xff);
    ASSERT_V(ev == RB_NULL, "putting extra byte properly failed");

    uint8_t type;
    uint8_t bytes;
    ev = rb_first_event(rb, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == TYPE, "type properly returned");
    ASSERT_V(bytes == SIZE, "size properly returned");

    rb_ptr_t p = ev;

    uint8_t u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p != RB_NULL, "rb_get_uint8 succeeded");
    ASSERT_V(u8 == 0x11, "rb_get_uint8 returned correct data");

    uint16_t u16 = 0;
    p = rb_get_uint16(rb, p, &u16);
    ASSERT_V(p != RB_NULL, "rb_get_uint16 succeeded");
    ASSERT_V(u16 == 0x2233, "rb_get_uint16 returned correct data");

    uint32_t u32 = 0;
    p = rb_get_uint32(rb, p, &u32);
    ASSERT_V(p != RB_NULL, "rb_get_uint32 succeeded");
    ASSERT_V(u32 == 0x44556677, "rb_get_uint32 returned correct data");

    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p == RB_NULL, "rb_get_uint8 properly failed at end of event");

    ev = rb_next_event(rb, ev, &type, &bytes);
    ASSERT_V(ev == RB_NULL, "rb_next_event returned RB_NULL on no more events");

    rb_destroy(rb);
}

DECLARE_TEST(rb_multiple_events, "multiple events with no deletions") {
    ring_buffer_t *rb = rb_create(32, NULL, NULL);

    rb_ptr_t ev = rb_alloc_event(rb, 1, 1);
    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x11);
    ASSERT_V(ev != RB_NULL, "put byte succeeded");

    ev = rb_alloc_event(rb, 2, 1);
    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x22);
    ASSERT_V(ev != RB_NULL, "put byte succeeded");

    ev = rb_alloc_event(rb, 3, 1);
    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x33);
    ASSERT_V(ev != RB_NULL, "put byte succeeded");

    uint8_t type;
    uint8_t bytes;
    ev = rb_first_event(rb, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == 1, "type properly returned");
    ASSERT_V(bytes == 1, "size properly returned");

    rb_ptr_t p = ev;

    uint8_t u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p != RB_NULL, "rb_get_uint8 succeeded");
    ASSERT_V(u8 == 0x11, "rb_get_uint8 returned correct data");

    ev = rb_next_event(rb, ev, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == 2, "type properly returned");
    ASSERT_V(bytes == 1, "size properly returned");

    p = ev;

    u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p != RB_NULL, "rb_get_uint8 succeeded");
    ASSERT_V(u8 == 0x22, "rb_get_uint8 returned correct data");

    ev = rb_next_event(rb, ev, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == 3, "type properly returned");
    ASSERT_V(bytes == 1, "size properly returned");

    p = ev;

    u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p != RB_NULL, "rb_get_uint8 succeeded");
    ASSERT_V(u8 == 0x33, "rb_get_uint8 returned correct data");

    ev = rb_next_event(rb, ev, &type, &bytes);
    ASSERT_V(ev == RB_NULL, "rb_next_event returned RB_NULL on no more events");

    rb_destroy(rb);
}

DECLARE_TEST(rb_empty_events, "empty events") {
    ring_buffer_t *rb = rb_create(32, NULL, NULL);

    rb_ptr_t ev = rb_alloc_event(rb, 1, 1);
    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x11);
    ASSERT_V(ev != RB_NULL, "put byte succeeded");

    ev = rb_alloc_event(rb, 2, 0);
    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x22);
    ASSERT_V(ev == RB_NULL, "put byte failed on empty event");

    ev = rb_alloc_event(rb, 3, 1);
    ASSERT_V(ev != RB_NULL, "event allocation succeeded");

    ev = rb_put_uint8(rb, ev, 0x33);
    ASSERT_V(ev != RB_NULL, "put byte succeeded");

    uint8_t type;
    uint8_t bytes;
    ev = rb_first_event(rb, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == 1, "type properly returned");
    ASSERT_V(bytes == 1, "size properly returned");

    rb_ptr_t p = ev;

    uint8_t u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p != RB_NULL, "rb_get_uint8 succeeded");
    ASSERT_V(u8 == 0x11, "rb_get_uint8 returned correct data");

    ev = rb_next_event(rb, ev, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == 2, "type properly returned");
    ASSERT_V(bytes == 0, "size of empty event properly returned");

    p = ev;

    u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p == RB_NULL, "rb_get_uint8 properly failed on empty event");

    ev = rb_next_event(rb, ev, &type, &bytes);

    ASSERT_V(ev != RB_NULL, "rb_first_event succeeded");
    ASSERT_V(type == 3, "type properly returned");
    ASSERT_V(bytes == 1, "size properly returned");

    p = ev;

    u8 = 0;
    p = rb_get_uint8(rb, p, &u8);
    ASSERT_V(p != RB_NULL, "rb_get_uint8 succeeded");
    ASSERT_V(u8 == 0x33, "rb_get_uint8 returned correct data");

    ev = rb_next_event(rb, ev, &type, &bytes);
    ASSERT_V(ev == RB_NULL, "rb_next_event returned RB_NULL on no more events");

    rb_destroy(rb);
}

static const int MAX_DEL_BYTES = 8;

typedef struct delete_t {
    uint8_t type;
    uint8_t len;
    uint8_t bytes[MAX_DEL_BYTES];
} delete_t;

static const int MAX_DELETES = 16;

typedef struct deletes_t {
    ring_buffer_t *rb;
    int cnt;
    delete_t dels[MAX_DELETES];
} deletes_t;

static void rb_head_cannot_reach_tail_cb(void *ctx, uint8_t type, uint8_t len, rb_ptr_t ev) {
    deletes_t *dels = ctx;
    ring_buffer_t *rb = dels->rb;
    delete_t *del = (dels->cnt < MAX_DELETES) ? dels->dels + dels->cnt : NULL;
    dels->cnt++;

    if (del) {
        del->type = type;
        del->len = len;
        int cnt = MIN(MAX_DEL_BYTES, del->len);
        for (int i = 0; i < cnt; i++) {
            ev = rb_get_uint8(rb, ev, del->bytes + i);
            if (ev == RB_NULL) {
                break;
            }
        }
    }
}

DECLARE_TEST(rb_head_cannot_reach_tail, "tail event is deleted if head would reach tail") {
    deletes_t dels;
    memset(&dels, 0, sizeof(dels));

    ring_buffer_t *rb = rb_create(16, &rb_head_cannot_reach_tail_cb, &dels);
    dels.rb = rb;

    /* there is two byte overhead per event so these should consume 12 bytes */
    rb_ptr_t ev[4];
    for (int i = 0; i < 3; i++) {
        ev[i] = rb_alloc_event(rb, i+1, sizeof(uint16_t));
        ASSERT_V(ev[i] != RB_NULL, "allocated event");
        rb_ptr_t p = rb_put_uint16(rb, ev[i], i+1);
        ASSERT_V(p != RB_NULL, "populated event");
    }

    ASSERT_V(dels.cnt == 0, "did not call delete until out of room");

    ev[3] = rb_alloc_event(rb, 4, sizeof(uint16_t));
    ASSERT_V(dels.cnt == 1, "deleted one item to make room");
    ASSERT_V(dels.dels[0].type == 1, "removed item had correct type");

    uint16_t data = *(uint16_t *)dels.dels[0].bytes;
    ASSERT_V(data == 1, "removed item had correct data");



}
