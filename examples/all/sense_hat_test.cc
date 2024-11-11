// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <platform-gpio.hh>
#include "../../libraries/sense_hat.hh"
#include "../../third_party/display_drivers/core/m3x6_16pt.h"
#include <thread.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Sense HAT">;
using Colour = SenseHat::Colour;

const Colour OnColour = {.red = Colour::MaxRedValue, .green = 0, .blue = 0}; 
const Colour OffColour = {.red = 25, .green = 25, .blue = 25};
constexpr uint64_t GolFrameWaitMsec = 400; 

constexpr char DemoText[] = "Running using CHERIoT on Sonata! ";
constexpr uint64_t TextFrameWaitMsec = 150; 

// Use a custom bitmap for "g" to make the character look better
constexpr uint8_t g_Bitmap[8] = {
    // @103 'g' (4 pixels wide)
    0x00,  //
    0x06,  //  ##
    0x05,  // # #
    0x07,  // ###
    0x04,  //   #
    0x03,  // ##
	0x00,  //
	0x00,  //
};

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

constexpr bool Blank[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

enum class Demo : uint8_t {
	GameOfLife = 0,
	ScrollingText = 1,
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

void update_text_state(bool state[8][8], uint32_t *index, uint32_t *column) {
	uint8_t currentChar = static_cast<uint8_t>(DemoText[*index]);
	// If at the end of the string, reset to the beginning (wrap around)
	if (currentChar == '\0') {
		*index = 0;
		*column = 0;
		currentChar = static_cast<uint8_t>(DemoText[0]);
	}
	// Copy all columns left by 1.
	for (uint8_t x = 0; x < 7; x++) {
		for (uint8_t y = 0; y < 8; y++) {
			state[y][x] = state[y][x+1];
		}
	}
	// Render the bitmap on the last column (7).
	// Hacky: detect gaps (columns with no zeroes) to determine
	// when the character ends, so that we can render this monospaced
	// font without monospacing.
	bool noZeroes = true;
	for (uint8_t y = 1; y < 8; y++) {
		const uint32_t bitmapAddr = (currentChar-32) * 8 + y - 1;
		const uint8_t bit = 1 << *column;
		if (currentChar == 'g') {
			// If 'g', use the custom bitmap
			state[y][7] = (g_Bitmap[y-1] & bit) >> *column;
		} else {
			state[y][7] = (m3x6_16ptBitmaps[bitmapAddr] & bit) >> *column;
		}
		if (state[y][7]) {
			noZeroes = false;
		}
	}
	// If a gap is found, or the entire character is rendered, progress
	// to the next character to render.
	if ((noZeroes && (currentChar != ' ')) || (*column == 6)) {
		if (++*index > sizeof(DemoText)) {
			*index = 0;
		}
		*column = 0;
	} else {
		*column += 1;
	}
}

/// Thread entry point.
void __cheri_compartment("sense_hat_test") test()
{
	Debug::log("Starting Sense HAT test");
	Demo currentDemo = Demo::GameOfLife;

	// Initialise GPIO capability for primitive joystick inputs
	auto gpio = MMIO_CAPABILITY(SonataGpioBoard, gpio_board);

	// Initialise the Sense HAT
	auto senseHat = SenseHat();

	// Initialise a blank LED Matrix.
	Colour fb[64] = {OffColour};
	senseHat.set_pixels(fb);

	bool gameState[8][8];
	if (currentDemo == Demo::GameOfLife) {
		// The starting seed for the 8x8 game of life.
		copy_state(Octagon2, gameState);
	} else {
		// The blank start for the wrapping text render.
		copy_state(Blank, gameState);
	}
	uint32_t index = 0;
	uint32_t column = 0;
	bool pressedJoystick = false;

	while (true) {;
		// If a joystick press is detected on an update, switch the demo type.
		// Not waited upon, so a joystick press can be missed between updates.
		bool joystickIsPressed = static_cast<uint16_t>(gpio->read_joystick()) & static_cast<uint16_t>(SonataJoystick::Pressed);
		if (!pressedJoystick && joystickIsPressed) {
			currentDemo = static_cast<Demo>((static_cast<uint8_t>(currentDemo) + 1) % 2);
			thread_millisecond_wait(500);
			if (currentDemo == Demo::GameOfLife) {
				copy_state(Octagon2, gameState);
			} else if (currentDemo == Demo::ScrollingText) {
				copy_state(Blank, gameState);
				column = 0;
				index = 0;
			}
			pressedJoystick = true;
		}
		if (pressedJoystick && !joystickIsPressed) {
			pressedJoystick = false;
		}

		if (currentDemo == Demo::GameOfLife) {
			// Every frame update the game state and image.
			thread_millisecond_wait(GolFrameWaitMsec);
			update_image(gameState, fb);
			senseHat.set_pixels(fb);
			update_gol_state(gameState);
		}
		else if (currentDemo == Demo::ScrollingText) {
			// Every frame update scrolling text and image
			thread_millisecond_wait(TextFrameWaitMsec);
			update_image(gameState, fb);
			senseHat.set_pixels(fb);
			update_text_state(gameState, &index, &column);			
		}
	}
}
