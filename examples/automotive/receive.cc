// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <platform-ethernet.hh>

#include "../../libraries/lcd.hh"

#include "./automotive.c"

using Debug = ConditionalDebug<true, "Automotive-Receive">;
using namespace CHERI;
using namespace sonata::lcd;

EthernetDevice *ethernet;
SonataLcd *lcd;

TaskOne car_info;

bool flag_reset = false;

uint64_t wait(const uint64_t wait_for) {
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);
    uint64_t cur_time = rdcycle64();
    while (cur_time < wait_for) {
        cur_time = rdcycle64();
        if (static_cast<uint8_t>(gpio->read_joystick()) & 
            static_cast<uint8_t>(SonataJoystick::Pressed)) {
            flag_reset = true;
        }
        // Busy wait, TODO could change to thread sleep?
    }
    return cur_time;
}

void size_t_to_str_base10(char *buffer, size_t num, uint8_t lpad, uint8_t rpad)
{
	// Parse the digits using repeated remainders mod 10
	ptrdiff_t endIdx = 0;
    while (rpad-- > 0) {
        buffer[endIdx++] = ' ';
    }
	if (num == 0)
	{
		buffer[endIdx++] = '0';
	}
	while (num != 0)
	{
		int remainder    = num % 10;
		buffer[endIdx++] = '0' + remainder;
		num /= 10;
	}
    while (lpad-- > 0) {
        buffer[endIdx++] = ' ';
    }
	buffer[endIdx--] = '\0';

	// Reverse the generated string
	ptrdiff_t startIdx = 0;
	while (startIdx < endIdx)
	{
		char swap          = buffer[startIdx];
		buffer[startIdx++] = buffer[endIdx];
		buffer[endIdx--]   = swap;
	}
}

void receive_ethernet_frame() {
    Debug::log("Polling for ethernet frame...");
    std::optional<EthernetDevice::Frame> maybeFrame = ethernet->receive_frame();
    if (!maybeFrame.has_value()) {
        return;
    }
    Debug::log("Received a frame with some value!");
    // TODO get length right and change to length, and do this nicer
    // TODO don't hardcode this
    //for (uint32_t i = 0; i < 16; ++i) {
    //    Debug::log("maybeFrame->buffer[{}] = {}", (int) i, maybeFrame->buffer[i]);
    //}
    car_info.acceleration =  ((uint64_t) maybeFrame->buffer[14]) << 56;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[15]) << 48;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[16]) << 40;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[17]) << 32;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[18]) << 24;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[19]) << 16;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[20]) << 8;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[21]);
    car_info.braking =  ((uint64_t) maybeFrame->buffer[22]) << 56;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[23]) << 48;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[24]) << 40;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[25]) << 32;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[26]) << 24;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[27]) << 16;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[28]) << 8;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[29]);
}

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	Debug::log(
	  "Unexpected CHERI Capability violation encountered. Stopping...");
	return ErrorRecoveryBehaviour::ForceUnwind;
}

void update_speed_estimate() {
    static uint32_t partial;
    if (car_info.acceleration > 0) {
        partial += (car_info.acceleration / 10);
        if (partial > 10) {
            car_info.speed += partial / 10;
            partial = partial % 10;
        }
    } else if (car_info.speed > 0) {
        car_info.speed -= 1;
    }
    if (car_info.braking > 0) {
        car_info.speed -= (car_info.braking / 3);
    }
}

// Thread entry point.
[[noreturn]] void __cheri_compartment("automotive_receive") entry() {
    // Initialise ethernet driver for use via callback
    ethernet = new EthernetDevice();
    ethernet->mac_address_set({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB});
    //while (!ethernet->phy_link_status()) {
    //    thread_millisecond_wait(50);
    //}
    lcd = new SonataLcd();
    Size  displaySize = lcd->resolution();
    Point centre      = {displaySize.width / 2, displaySize.height / 2};
    lcd->clean(Color::Black);
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);

    const uint32_t cyclesPerMillisecond = CPU_TIMER_HZ / 1000;
    const uint32_t waitTime = 80 * cyclesPerMillisecond;
    uint64_t prev_time = rdcycle64();
    while (true) {
        update_speed_estimate(); // TODO remove when actual car is in
        receive_ethernet_frame();
        Debug::log("Current pedal info - acceleration={}, braking={}", (int) car_info.acceleration, (int) car_info.braking);
        char accelerationStr[50];
        memcpy(accelerationStr, "Acceleration: ", 14);
        size_t_to_str_base10(&accelerationStr[14], (size_t) car_info.acceleration, 0, 10);
        char brakingStr[50];
        memcpy(brakingStr, "Braking: ", 9);
        size_t_to_str_base10(&brakingStr[9], (size_t) car_info.braking, 0, 10);
        char speedStr[50];
        memcpy(speedStr, "Estimated Speed: ", 17);
        size_t_to_str_base10(&speedStr[17], (size_t) car_info.speed, 0, 10);
        lcd->draw_str({centre.x - 55, centre.y - 40}, "Press the joystick to reset!", 
            Color::Black, Color::White);
        lcd->draw_str({centre.x - 48, centre.y - 10}, accelerationStr, Color::Black, 
            (car_info.acceleration > 100) ? Color::Red : Color::White);
        lcd->draw_str({centre.x - 48, centre.y}, brakingStr, Color::Black, 
            Color::White);
        lcd->draw_str({centre.x - 48, centre.y + 10}, speedStr, Color::Black, 
            (car_info.acceleration > 100) ? Color::Red : Color::White);
        prev_time = wait(prev_time + waitTime);
        if (flag_reset |
            (static_cast<uint8_t>(gpio->read_joystick()) & 
             static_cast<uint8_t>(SonataJoystick::Pressed))) {
            flag_reset = false;
            car_info.acceleration = 0;
            car_info.braking = 0;
            car_info.speed = 0;
        }
    }

    // Cleanup
    delete ethernet;
    delete lcd;

}
