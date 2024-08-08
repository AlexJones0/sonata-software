// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <debug.hh>
#include <thread.h>

#include "bad.hh"

using Debug = ConditionalDebug<true, "Automotive">;

#define NUM_UPDATES 200
#define UPDATE_SLEEP_TIME_MSEC 50

typedef struct CarInformation {
	uint64_t write_info[128];
	uint64_t acceleration;
	uint64_t braking;
	uint64_t speed;
} CarInformation;

// Thread entry point.
[[noreturn]] void __cheri_compartment("automotive") automotive_entry()
{
	Debug::log("Automotive demo started!");
	CarInformation *car_info = (CarInformation *) malloc(sizeof(CarInformation));
	car_info->acceleration = 0;
	car_info->braking = 0;
	car_info->speed = 30;
	bad_start(car_info->write_info);

	while (true) {
		bad_update();
		Debug::log("Est. car speed: {}. Sending: acceleration={}, braking={}", 
				   (int) car_info->speed,
				   (int) car_info->acceleration, 
				   (int) car_info->braking);
		car_info->speed += (car_info->acceleration > 0) ? 1 : 0;
		thread_millisecond_wait(UPDATE_SLEEP_TIME_MSEC);
	}
}
