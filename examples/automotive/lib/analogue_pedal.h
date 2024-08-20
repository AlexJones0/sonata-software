// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef AUTOMOTIVE_ANALOGUE_PEDAL_H
#define AUTOMOTIVE_ANALOGUE_PEDAL_H

#include <stdbool.h>
#include <stdint.h>

#include "automotive_common.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
void init_analogue_pedal_demo_mem(TaskOne *task_one, TaskTwo *task_two);
void run_analogue_pedal_demo(uint64_t init_time);
#ifdef __cplusplus
}
#endif //__cplusplus

#endif // AUTOMOTIVE_ANALOGUE_PEDAL_H
