// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>

using Debug = ConditionalDebug<true, "Bad">;
using namespace CHERI;

uint64_t counter = 0;
uint64_t *arr = NULL;

void __cheri_compartment("bad") bad_update()
{
	++counter;
	if (counter >= 100) {
		Debug::log("Update called 100+ times - crashing...");
	} else {
		Debug::log("Update called {} times", (int) counter);
	}
	if (counter <= 100) {
		// Oops! I put "<=" instead of "<", so this should accidentally
		// write into the car's acceleration value.
		arr[counter] = 1000;
	}
}

void __cheri_compartment("bad") bad_start(uint64_t *write_arr)
{
	arr = write_arr;
	Debug::log("Start called!");
}
