#include "scheduler.h"

#include <stdatomic.h>
#include <string.h>

#define EVENT_MASK (SCHED_EVENT_CAPACITY - 1u)

static bool reached(uint32_t now, uint32_t due) {
    return (int32_t)(now - due) >= 0;
}

void sched_init(scheduler_t *scheduler, uint32_t start_tick) {
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->tick_ms = start_tick;
}

bool sched_add_task(scheduler_t *scheduler, sched_task_fn run, void *context,
                    uint32_t period_ms, uint32_t phase_ms,
                    uint32_t deadline_ms, uint8_t priority) {
    if (scheduler == 0 || run == 0 || period_ms == 0u ||
        period_ms > INT32_MAX || phase_ms > INT32_MAX ||
        deadline_ms == 0u || deadline_ms > period_ms ||
        scheduler->task_count >= SCHED_MAX_TASKS) return false;
    sched_task_t *task = &scheduler->tasks[scheduler->task_count++];
    task->run = run;
    task->context = context;
    task->period_ms = period_ms;
    task->deadline_ms = deadline_ms;
    task->next_release = scheduler->tick_ms + phase_ms;
    task->priority = priority;
    return true;
}

void sched_tick_isr(scheduler_t *scheduler) { scheduler->tick_ms++; }

bool sched_post_event_isr(scheduler_t *scheduler, uint8_t event) {
    uint8_t next = (uint8_t)((scheduler->event_head + 1u) & EVENT_MASK);
    if (next == scheduler->event_tail) {
        scheduler->dropped_events++;
        return false;
    }
    scheduler->events[scheduler->event_head] = event;
    atomic_signal_fence(memory_order_release);
    scheduler->event_head = next;
    return true;
}

bool sched_take_event(scheduler_t *scheduler, uint8_t *event) {
    if (event == 0 || scheduler->event_tail == scheduler->event_head) return false;
    atomic_signal_fence(memory_order_acquire);
    *event = scheduler->events[scheduler->event_tail];
    scheduler->event_tail = (uint8_t)((scheduler->event_tail + 1u) & EVENT_MASK);
    return true;
}

bool sched_dispatch_one(scheduler_t *scheduler) {
    uint32_t now = scheduler->tick_ms;
    sched_task_t *chosen = 0;
    for (unsigned i = 0; i < scheduler->task_count; ++i) {
        sched_task_t *task = &scheduler->tasks[i];
        if (reached(now, task->next_release)) {
            uint32_t releases = (now - task->next_release) / task->period_ms + 1u;
            if (task->ready) task->missed_releases += releases;
            else {
                task->ready = true;
                task->released_at = task->next_release;
                task->missed_releases += releases - 1u;
            }
            task->next_release += releases * task->period_ms;
        }
        if (task->ready &&
            (chosen == 0 || task->priority < chosen->priority)) chosen = task;
    }
    if (chosen == 0) {
        scheduler->idle_dispatches++;
        return false;
    }
    chosen->ready = false;
    if (reached(now, chosen->released_at + chosen->deadline_ms) &&
        now != chosen->released_at + chosen->deadline_ms)
        chosen->missed_deadlines++;
    chosen->runs++;
    chosen->run(chosen->context, now);
    /* A long callback can also miss its deadline. */
    uint32_t after = scheduler->tick_ms;
    if (after != now && reached(after, chosen->released_at + chosen->deadline_ms) &&
        after != chosen->released_at + chosen->deadline_ms &&
        !reached(now, chosen->released_at + chosen->deadline_ms))
        chosen->missed_deadlines++;
    return true;
}
