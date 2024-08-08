// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>

using Debug = ConditionalDebug<true, "Bad">;
using namespace CHERI;

uint64_t counter = 0;
uint64_t *arr = NULL; 
size_t arr_size = 0;

void __cheri_compartment("bad") bad_update()
{
	++counter;
	if (counter >= 100) {
		Debug::log("Update called 100+ times - crashing...");
		arr[arr_size+1] = 1;
	} else {
		Debug::log("Update called {} times", (int) counter);
	}
}

void __cheri_compartment("bad") bad_start(size_t size)
{
	arr_size = size;
	arr = new uint64_t[arr_size];
	Debug::log("Start called!");
}

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	auto [exceptionCode, registerNumber] = extract_cheri_mtval(mtval);
	if (exceptionCode == CauseCode::BoundsViolation ||
	    exceptionCode == CauseCode::TagViolation)
	{
		// If an explicit out of bounds access occurs, or bounds are made
		// invalid by some negative array access, we **assume** that this was
		// caused by the SnakeGame::check_if_colliding function and that the
		// snake has hit the boundary of the game and so the game should end.
		Debug::log("Crash caught!");
	}
	return ErrorRecoveryBehaviour::ForceUnwind;
}
