// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <platform-gpio.hh>
#include "../../libraries/sense_hat.hh"
#include <thread.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Sense HAT">;
using Colour = SenseHat::Colour;

const Colour OnColour = {.red = Colour::MaxRedValue, .green = 0, .blue = 0}; 
const Colour OffColour = {.red = 25, .green = 25, .blue = 25};
constexpr uint64_t FrameWaitMsec = 400; 

constexpr bool Mold[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 1, 1, 0, 0},
	{0, 0, 0, 1, 0, 0, 1, 0},
	{0, 0, 1, 0, 1, 0, 1, 0},
	{0, 0, 1, 0, 0, 1, 0, 0},
	{0, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

constexpr bool Octagon2[8][8] = {
	{0, 0, 0, 1, 1, 0, 0, 0},
	{0, 0, 1, 0, 0, 1, 0, 0},
	{0, 1, 0, 0, 0, 0, 1, 0},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 1},
	{0, 1, 0, 0, 0, 0, 1, 0},
	{0, 0, 1, 0, 0, 1, 0, 0},
	{0, 0, 0, 1, 1, 0, 0, 0},
};

constexpr bool Mazing[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 1, 1, 0, 0, 0},
	{0, 1, 0, 1, 0, 0, 0, 0},
	{1, 0, 0, 0, 0, 0, 1, 0},
	{0, 1, 0, 0, 0, 1, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 1, 0, 1, 0, 0},
	{0, 0, 0, 0, 1, 0, 0, 0},
};

void copy_state(const bool source[8][8], bool dest[8][8]) {
	for (uint8_t y = 0; y < 8; y++) {
		for (uint8_t x = 0; x < 8; x++) {
			dest[y][x] = source[y][x];
		}
	}
}

void update_image(const bool state[8][8], Colour fb[64]) {
	// Update the pixel framebuffer based on the current GOL game state
	for (uint8_t i = 0; i < 64; i++) {
		fb[i] = state[i/8][i%8] ? OnColour : OffColour;
	}
}

void update_gol_state(bool state[8][8]) {
	bool result[8][8] = {0};
	// Iterate through cells
	for (uint8_t y = 0; y < 8; y++) {
		for (uint8_t x = 0; x < 8; x++) {
			// Count neighbours (can be optimised but no need)
			uint8_t neighbours = 0;
			for (int8_t i = -1; i <= 1; i++) {
				for (int8_t j = -1; j <= 1; j++) {
					if (i == 0 && j == 0) continue;
					int8_t ny = y + i;
					int8_t nx = x + j;
					if (ny < 0 || ny >= 8 || nx < 0 || nx >= 8) continue;
					neighbours += state[ny][nx];
				}
			}
			// Calculate the new state and store into a temporary result array
			if (state[y][x] && neighbours != 2 && neighbours != 3) {
				result[y][x] = 0;
			} else if (!state[y][x] && neighbours == 3) {
				result[y][x] = 1;
			} else {
				result[y][x] = state[y][x];
			}
		}
	}
	// Copy the newly calculated state back into the original 2D array
	copy_state(result, state);
}

/// Thread entry point.
void __cheri_compartment("sense_hat_test") test()
{
	Debug::log("Starting Sense HAT test");

	// Initialise the Sense HAT
	auto senseHat = SenseHat();

	// Initialise a blank LED Matrix.
	Colour fb[64] = {OffColour};
	senseHat.set_pixels(fb);

	// The starting seed for the 8x8 game of life.
	bool gameState[8][8];
	copy_state(Octagon2, gameState);

	while (true) {
		// Every frame update the game state and image.
		thread_millisecond_wait(FrameWaitMsec);
		update_image(gameState, fb);
		senseHat.set_pixels(fb);
		update_gol_state(gameState);
	}
}
