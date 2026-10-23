#include "controller.h"
#include "scheduler.h"

#include <assert.h>
#include <stdio.h>

typedef struct { unsigned calls; unsigned order[8]; unsigned length; } fixture_t;
typedef struct { fixture_t *fixture; unsigned id; scheduler_t *scheduler; bool consume_time; } task_ctx_t;

static void task(void *context, uint32_t now) {
    task_ctx_t *t = context;
    t->fixture->calls++;
    if (t->fixture->length < 8u) t->fixture->order[t->fixture->length++] = t->id;
    if (t->consume_time) {
        sched_tick_isr(t->scheduler);
        sched_tick_isr(t->scheduler);
    }
    (void)now;
}

int main(void) {
    scheduler_t s;
    fixture_t f = {0};
    task_ctx_t low = {&f, 2, &s, false};
    task_ctx_t high = {&f, 1, &s, false};
    sched_init(&s, 0);
    assert(sched_add_task(&s, task, &low, 10, 0, 5, 2));
    assert(sched_add_task(&s, task, &high, 10, 0, 5, 0));
    assert(sched_dispatch_one(&s) && sched_dispatch_one(&s));
    assert(f.length == 2u && f.order[0] == 1u && f.order[1] == 2u);
    assert(!sched_dispatch_one(&s));
    for (unsigned i = 0; i < 10u; ++i) sched_tick_isr(&s);
    assert(sched_dispatch_one(&s) && s.tasks[1].runs == 2u);
    for (unsigned i = 0; i < 30u; ++i) sched_tick_isr(&s);
    assert(sched_dispatch_one(&s));
    assert(sched_dispatch_one(&s));
    assert(s.tasks[0].missed_releases >= 2u);
    assert(s.tasks[0].missed_deadlines >= 1u);

    /* One slot remains free so head and tail never alias when full. */
    sched_init(&s, 0);
    for (unsigned i = 0; i < SCHED_EVENT_CAPACITY - 1u; ++i)
        assert(sched_post_event_isr(&s, (uint8_t)i));
    assert(!sched_post_event_isr(&s, 99) && s.dropped_events == 1u);
    for (unsigned i = 0; i < SCHED_EVENT_CAPACITY - 1u; ++i) {
        uint8_t event;
        assert(sched_take_event(&s, &event) && event == i);
    }
    uint8_t event;
    assert(!sched_take_event(&s, &event));

    /* Signed time comparison remains valid across the 32-bit tick wrap. */
    sched_init(&s, UINT32_MAX - 2u);
    assert(sched_add_task(&s, task, &high, 10, 5, 5, 0));
    for (unsigned i = 0; i < 5u; ++i) {
        assert(!sched_dispatch_one(&s));
        sched_tick_isr(&s);
    }
    assert(sched_dispatch_one(&s));

    sched_init(&s, 0);
    task_ctx_t slow = {&f, 3, &s, true};
    assert(sched_add_task(&s, task, &slow, 10, 0, 1, 0));
    assert(sched_dispatch_one(&s) && s.tasks[0].missed_deadlines == 1u);

    controller_t c;
    controller_init(&c, 5000);
    assert(controller_step(&c, 2500, 10) > 0);
    controller_init(&c, 5000);
    for (unsigned i = 0; i < 10000u; ++i)
        assert(controller_step(&c, -3000, 10) <= 1000u);
    assert(c.integral_q8 == 0); /* no wind-up at upper saturation */
    controller_latch_fault(&c);
    assert(controller_step(&c, 2500, 10) == 0);
    controller_clear_fault(&c);
    assert(controller_step(&c, 2500, 10) > 0);
    assert(controller_step(&c, 20000, 10) == 0 && c.fault_latched);
    puts("scheduler and controller tests passed");
    return 0;
}
