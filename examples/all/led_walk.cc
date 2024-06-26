// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include "gpiolib.hh"
#include <debug.hh>
#include <thread.h>
#include <vector>

using Debug = ConditionalDebug<true, "Led Walk">;

/// Thread entry point.
void __cheri_compartment("led_walk") start_walking()
{
	std::vector<LedHandle *> leds;
	for (uint8_t num = 0; num < 8; ++num)
	{
		auto led = aquire_led(num);
		Debug::Assert(led.has_value(), "LED {} couldn't be aquired", num);
		leds.push_back(led.value());
	};

	Debug::log("          LED 3 Handle: {}", leds[3]);
	release_led(leds[3]);
	Debug::log("Destroyed LED 3 Handle: {}", leds[3]);
	leds[3] = aquire_led(3).value();
	Debug::log("      New LED 3 Handle: {}", leds[3]);

	while (true)
	{
		for (auto led : leds)
		{
			bool success = toggle_led(led);
			Debug::Assert(success, "Failed to toggle an LED");
			thread_millisecond_wait(500);
		}
	}
}
