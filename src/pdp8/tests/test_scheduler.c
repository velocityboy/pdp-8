#include "../src/scheduler.h"
#include "tests.h"

static void fn1(void *p) {}
static void fn2(void *p) {}
static void fn3(void *p) {}
static void fn4(void *p) {}
static void fn5(void *p) {}

DECLARE_TEST(sch_scheduler, "priority queue scheduler") {
    scheduler_t *sch = scheduler_create();

    scheduler_insert(sch, 3, &fn3, (void *)3);
    scheduler_insert(sch, 4, &fn4, (void *)4);
    scheduler_insert(sch, 1, &fn1, (void *)1);
    scheduler_insert(sch, 5, &fn5, (void *)5);
    scheduler_insert(sch, 2, &fn2, (void *)2);

    scheduler_callback_t callback;

    ASSERT_V(!scheduler_empty(sch), "scheduler is not yet empty");
    ASSERT_V(scheduler_next_event_time(sch) == 1, "scheduler peeked correct time");
    ASSERT_V(scheduler_extract_min(sch, &callback) == 0, "extracted min element");
    ASSERT_V(callback.time == 1, "correct time returned");
    ASSERT_V(callback.event == &fn1, "correct event handler returned");
    ASSERT_V(callback.ctx == (void *)1, "correct context returned");

    ASSERT_V(!scheduler_empty(sch), "scheduler is not yet empty");
    ASSERT_V(scheduler_next_event_time(sch) == 2, "scheduler peeked correct time");
    ASSERT_V(scheduler_extract_min(sch, &callback) == 0, "extracted min element");
    ASSERT_V(callback.time == 2, "correct time returned");
    ASSERT_V(callback.event == &fn2, "correct event handler returned");
    ASSERT_V(callback.ctx == (void *)2, "correct context returned");
    
    ASSERT_V(!scheduler_empty(sch), "scheduler is not yet empty");
    ASSERT_V(scheduler_next_event_time(sch) == 3, "scheduler peeked correct time");
    ASSERT_V(scheduler_extract_min(sch, &callback) == 0, "extracted min element");
    ASSERT_V(callback.time == 3, "correct time returned");
    ASSERT_V(callback.event == &fn3, "correct event handler returned");
    ASSERT_V(callback.ctx == (void *)3, "correct context returned");
    
    ASSERT_V(!scheduler_empty(sch), "scheduler is not yet empty");
    ASSERT_V(scheduler_next_event_time(sch) == 4, "scheduler peeked correct time");
    ASSERT_V(scheduler_extract_min(sch, &callback) == 0, "extracted min element");
    ASSERT_V(callback.time == 4, "correct time returned");
    ASSERT_V(callback.event == &fn4, "correct event handler returned");
    ASSERT_V(callback.ctx == (void *)4, "correct context returned");

    ASSERT_V(!scheduler_empty(sch), "scheduler is not yet empty");
    ASSERT_V(scheduler_next_event_time(sch) == 5, "scheduler peeked correct time");
    ASSERT_V(scheduler_extract_min(sch, &callback) == 0, "extracted min element");
    ASSERT_V(callback.time == 5, "correct time returned");
    ASSERT_V(callback.event == &fn5, "correct event handler returned");
    ASSERT_V(callback.ctx == (void *)5, "correct context returned");

    ASSERT_V(scheduler_empty(sch), "scheduler is empty");
    
    scheduler_free(sch);
}
