#include "controller.h"

#include <limits.h>

static int32_t clamp_i32(int64_t value, int32_t low, int32_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return (int32_t)value;
}

void controller_init(controller_t *controller, int32_t setpoint_centi_c) {
    controller->setpoint_centi_c = clamp_i32(setpoint_centi_c, -4000, 12500);
    controller->kp_q8 = 40; /* 0.15625 permille per centi-C */
    controller->ki_q8 = 4;  /* integral gain per second */
    controller->integral_q8 = 0;
    controller->output_permille = 0;
    controller->fault_latched = false;
}

uint16_t controller_step(controller_t *controller, int32_t measured_centi_c,
                         uint32_t dt_ms) {
    if (controller->fault_latched || measured_centi_c < -4000 ||
        measured_centi_c > 12500 || dt_ms == 0u || dt_ms > 1000u) {
        controller->output_permille = 0;
        controller->fault_latched = true;
        return 0;
    }
    int64_t error = (int64_t)controller->setpoint_centi_c - measured_centi_c;
    int64_t proportional = (int64_t)controller->kp_q8 * error;
    int64_t delta = (int64_t)controller->ki_q8 * error * dt_ms / 1000;
    int32_t proposed = clamp_i32((int64_t)controller->integral_q8 + delta,
                                 0, 1000 * 256);
    int64_t raw = proportional + proposed;
    /* Conditional integration prevents wind-up while saturated. */
    if (!((raw > 1000 * 256 && delta > 0) || (raw < 0 && delta < 0)))
        controller->integral_q8 = proposed;
    raw = proportional + controller->integral_q8;
    controller->output_permille = (uint16_t)clamp_i32(raw / 256, 0, 1000);
    return controller->output_permille;
}

void controller_latch_fault(controller_t *controller) {
    controller->fault_latched = true;
    controller->output_permille = 0;
    controller->integral_q8 = 0;
}
void controller_clear_fault(controller_t *controller) {
    controller->fault_latched = false;
    controller->integral_q8 = 0;
}
