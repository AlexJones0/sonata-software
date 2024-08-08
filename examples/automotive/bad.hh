// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef BAD_H
#define BAD_H

#include <compartment.h>

void __cheri_compartment("bad") bad_update();

void __cheri_compartment("bad") bad_start(size_t size);

void __cheri_compartment("bad") bad_stop();

#endif // ifndef BAD_H
