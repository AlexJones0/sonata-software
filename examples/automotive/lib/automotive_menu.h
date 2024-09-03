// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef AUTOMOTIVE_MENU_H
#define AUTOMOTIVE_MENU_H

#include <stdint.h>

typedef enum DemoApplication {
    AnaloguePedal = 0,
    DigitalPedal  = 1,
    JoystickPedal = 2,
    NoPedal       = 3,
} DemoApplication;

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
DemoApplication select_demo();
#ifdef __cplusplus
}
#endif //__cplusplus

#endif // AUTOMOTIVE_MENU_H
