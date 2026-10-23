#include "controller.h"
#include "scheduler.h"

#include <stdio.h>

enum { EVENT_SENSOR_FAULT = 1, EVENT_CLEAR_FAULT = 2 };

typedef struct {
    scheduler_t scheduler;
    controller_t controller;
    int32_t temperature_centi_c;
    int32_t plant_subcenti;
    unsigned logs;
} simulation_t;

static void safety_task(void *context, uint32_t now) {
    simulation_t *sim = context;
    uint8_t event;
    while (sched_take_event(&sim->scheduler, &event)) {
        if (event == EVENT_SENSOR_FAULT) controller_latch_fault(&sim->controller);
        if (event == EVENT_CLEAR_FAULT) controller_clear_fault(&sim->controller);
    }
    if (sim->temperature_centi_c > 7000)
        controller_latch_fault(&sim->controller);
    (void)now;
}

static void control_task(void *context, uint32_t now) {
    simulation_t *sim = context;
    (void)controller_step(&sim->controller, sim->temperature_centi_c, 10);
    (void)now;
}

static void status_task(void *context, uint32_t now) {
    simulation_t *sim = context;
    if (sim->logs % 10u == 0u) {
        printf("%5lu ms  temp=%ld.%02ld C  duty=%u/1000  fault=%u\n",
               (unsigned long)now,
               (long)(sim->temperature_centi_c / 100),
               (long)(sim->temperature_centi_c % 100),
               sim->controller.output_permille,
               sim->controller.fault_latched ? 1u : 0u);
    }
    sim->logs++;
}

int main(void) {
    simulation_t sim = {0};
    sched_init(&sim.scheduler, 0);
    controller_init(&sim.controller, 5000); /* 50 C target */
    sim.temperature_centi_c = 2500;
    if (!sched_add_task(&sim.scheduler, safety_task, &sim, 20, 0, 5, 0) ||
        !sched_add_task(&sim.scheduler, control_task, &sim, 10, 0, 5, 1) ||
        !sched_add_task(&sim.scheduler, status_task, &sim, 100, 0, 20, 2))
        return 1;
    for (unsigned ms = 0; ms < 30000u; ++ms) {
        if (ms == 18000u) (void)sched_post_event_isr(&sim.scheduler, EVENT_SENSOR_FAULT);
        if (ms == 22000u) (void)sched_post_event_isr(&sim.scheduler, EVENT_CLEAR_FAULT);
        /* Pure software plant: heating versus ambient cooling. */
        sim.plant_subcenti += (int32_t)sim.controller.output_permille -
                              (sim.temperature_centi_c - 2500) / 20;
        sim.temperature_centi_c += sim.plant_subcenti / 1000;
        sim.plant_subcenti %= 1000;
        (void)sched_dispatch_one(&sim.scheduler);
        sched_tick_isr(&sim.scheduler); /* virtual 1 ms timer interrupt */
    }
    printf("runs safety=%lu control=%lu status=%lu; missed deadlines=%lu/%lu/%lu\n",
           (unsigned long)sim.scheduler.tasks[0].runs,
           (unsigned long)sim.scheduler.tasks[1].runs,
           (unsigned long)sim.scheduler.tasks[2].runs,
           (unsigned long)sim.scheduler.tasks[0].missed_deadlines,
           (unsigned long)sim.scheduler.tasks[1].missed_deadlines,
           (unsigned long)sim.scheduler.tasks[2].missed_deadlines);
    return 0;
}
