// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <utility>
#include <debug.hh>

namespace Internal {
	void __cheri_libcall init_i2c();
}

class SenseHat {
	private:
	public:

	/// Flag set when we're debugging this driver.
	static constexpr bool DebugSenseHat = true;

	/// Helper for conditional debug logs and assertions.
	using Debug = ConditionalDebug<DebugSenseHat, "Sense HAT">;

	struct Colour {
		// Sense HAT LED Matrix uses RGB 565
		static constexpr uint8_t RedBits = 5;
		static constexpr uint8_t GreenBits = 6;
		static constexpr uint8_t BlueBits = 5;

		// Maximum values for each field, calculated from the number of bits
		static constexpr uint8_t MaxRedValue = (1 << RedBits) - 1;
		static constexpr uint8_t MaxGreenValue = (1 << GreenBits) - 1;
		static constexpr uint8_t MaxBlueValue = (1 << BlueBits) - 1;

		// Colour fields
		uint8_t red;
		uint8_t green;
		uint8_t blue;
	} __attribute__((packed));

	SenseHat() {
		Internal::init_i2c();
	}

	bool __cheri_libcall set_pixels(Colour pixels8x8[64]);
};

//uint16_t rgb_to_rgb565(uint8_t red, uint8_t green, uint8_t blue) {
//	return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
//}