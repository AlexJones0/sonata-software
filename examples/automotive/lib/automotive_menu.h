// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef AUTOMOTIVE_MENU_H
#define AUTOMOTIVE_MENU_H

#include <stdint.h>

typedef enum DemoApplication {
    NoPedal = 0,
    JoystickPedal = 1,
    DigitalPedal = 2,
    AnaloguePedal = 3,
} DemoApplication;

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
DemoApplication select_demo();
#ifdef __cplusplus
}
#endif //__cplusplus

#endif // AUTOMOTIVE_MENU_H
