#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t setpoint_centi_c;
    int32_t kp_q8;
    int32_t ki_q8;
    int32_t integral_q8;
    uint16_t output_permille;
    bool fault_latched;
} controller_t;

void controller_init(controller_t *controller, int32_t setpoint_centi_c);
/* Fixed-point PI: gains are Q8, input is centi-C, output is 0..1000 permille. */
uint16_t controller_step(controller_t *controller, int32_t measured_centi_c,
                         uint32_t dt_ms);
void controller_latch_fault(controller_t *controller);
void controller_clear_fault(controller_t *controller);

#endif
