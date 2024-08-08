// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef BAD_H
#define BAD_H

#include <compartment.h>

void __cheri_compartment("bad") bad_update();

void __cheri_compartment("bad") bad_start(uint64_t *write_arr);

void __cheri_compartment("bad") bad_stop();

#endif // ifndef BAD_H
