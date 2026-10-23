#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#define SCHED_MAX_TASKS 6u
#define SCHED_EVENT_CAPACITY 16u

typedef void (*sched_task_fn)(void *context, uint32_t now_ms);

typedef struct {
    sched_task_fn run;
    void *context;
    uint32_t period_ms;
    uint32_t deadline_ms;
    uint32_t next_release;
    uint32_t released_at;
    uint32_t runs;
    uint32_t missed_releases;
    uint32_t missed_deadlines;
    uint8_t priority; /* smaller number runs first */
    bool ready;
} sched_task_t;

typedef struct {
    volatile uint32_t tick_ms;
    sched_task_t tasks[SCHED_MAX_TASKS];
    uint8_t task_count;
    volatile uint8_t event_head;
    volatile uint8_t event_tail;
    volatile uint32_t dropped_events;
    uint8_t events[SCHED_EVENT_CAPACITY];
    uint32_t idle_dispatches;
} scheduler_t;

void sched_init(scheduler_t *scheduler, uint32_t start_tick);
bool sched_add_task(scheduler_t *scheduler, sched_task_fn run, void *context,
                    uint32_t period_ms, uint32_t phase_ms,
                    uint32_t deadline_ms, uint8_t priority);
/* Single timer ISR and single foreground consumer; Cortex-M aligned 32-bit tick is atomic. */
void sched_tick_isr(scheduler_t *scheduler);
bool sched_post_event_isr(scheduler_t *scheduler, uint8_t event);
bool sched_take_event(scheduler_t *scheduler, uint8_t *event);
/* Releases due jobs and executes at most one, keeping foreground work bounded. */
bool sched_dispatch_one(scheduler_t *scheduler);

#endif
