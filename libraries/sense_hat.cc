// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include "sense_hat.hh"
#include <cheri.hh>
#include <platform-i2c.hh>

using namespace CHERI;

/**
 * Helper. Returns a pointer to the I2C device.
 */
[[nodiscard, gnu::always_inline]] static Capability<volatile OpenTitanI2c> i2c()
{
	return MMIO_CAPABILITY(OpenTitanI2c, i2c1);
}

void __cheri_libcall Internal::init_i2c() {
	i2c()->reset_fifos();
	i2c()->host_mode_set();
	i2c()->speed_set(100);
}

bool __cheri_libcall SenseHat::set_pixels(Colour pixels8x8[64]) {
	uint8_t write_buffer[1+64*3] = {0x0};
	uint32_t i = 0;
	write_buffer[i++] = 0x00; // Address
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t column = 0; column < 8; column++) {
			uint32_t index = row * 8 + column;
			if (pixels8x8[index].red > Colour::MaxRedValue) {
				Debug::log("Pixel at row {}, column {} exceeds maximum red.",
					row, column);
				return false;
			}
			write_buffer[i++] = pixels8x8[index].red;
		}
		for (uint8_t column = 0; column < 8; column++) {
			uint32_t index = row * 8 + column;
			if (pixels8x8[index].green > Colour::MaxGreenValue) {
				Debug::log("Pixel at row {}, column {} exceeds maximum green.",
					row, column);
				return false;
			}
			write_buffer[i++] = pixels8x8[index].green;
		}
		for (uint8_t column = 0; column < 8; column++) {
			uint32_t index = row * 8 + column;
			if (pixels8x8[index].blue > Colour::MaxBlueValue) {
				Debug::log("Pixel at row {}, column {} exceeds maximum blue.",
					row, column);
				return false;
			}
			write_buffer[i++] = pixels8x8[index].blue;
		}
	}
	if (!i2c()->blocking_write(0x46, write_buffer, sizeof(write_buffer), true)) {
		return false;
	}
	return true;
}