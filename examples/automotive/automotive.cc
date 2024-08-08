// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <debug.hh>
#include <thread.h>

#include "bad.hh"

using Debug = ConditionalDebug<true, "Automotive">;

#define NUM_UPDATES 200
#define UPDATE_SLEEP_TIME_MSEC 50
#define ARR_SIZE 50

// Thread entry point.
[[noreturn]] void __cheri_compartment("automotive") automotive_entry()
{
	Debug::log("Automotive demo started!");
	bad_start(ARR_SIZE);

	while (true) {
		bad_update();
		Debug::log("Sending important accelaration information!");
		thread_millisecond_wait(UPDATE_SLEEP_TIME_MSEC);
	}
}
