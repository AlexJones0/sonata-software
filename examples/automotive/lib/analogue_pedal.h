// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef AUTOMOTIVE_ANALOGUE_PEDAL_H
#define AUTOMOTIVE_ANALOGUE_PEDAL_H

#include <stdbool.h>
#include <stdint.h>

#include "automotive_common.h"

#define DEMO_ACCELERATION_PEDAL_MIN 0
#define DEMO_ACCELERATION_PEDAL_MAX 50 // Limit to 50 out of 100 so that the speedup can actually be observed

typedef struct AnalogueTaskOne {
    uint64_t acceleration;
    uint64_t braking;
} AnalogueTaskOne;

typedef struct AnalogueTaskTwo {
    uint64_t volume;
    uint64_t framebuffer[20];
} AnalogueTaskTwo;

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
void init_analogue_pedal_demo_mem(AnalogueTaskOne *task_one, AnalogueTaskTwo *task_two);
void run_analogue_pedal_demo(uint64_t init_time);
#ifdef __cplusplus
}
#endif //__cplusplus

#endif // AUTOMOTIVE_ANALOGUE_PEDAL_H
