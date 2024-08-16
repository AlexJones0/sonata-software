// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <platform-ethernet.hh>

#include "../../libraries/lcd.hh"

#include "./lib/simple_bug.h"

using Debug = ConditionalDebug<true, "Automotive-Receive">;
using namespace CHERI;
using namespace sonata::lcd;

EthernetDevice *ethernet;
SonataLcd *lcd;

TaskOne car_info;

bool flag_reset = false;

typedef uint32_t* pwm_t;

#define DEV_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = val)
#define DEV_READ(addr) (*((volatile uint32_t *)(addr)))

#define PWM_FROM_ADDR_AND_INDEX(addr, index) (&(((pwm_t)addr)[2 * index]))

void set_pwm(pwm_t pwm, uint32_t counter, uint32_t pulse_width) {
  DEV_WRITE(&pwm[1], counter);
  DEV_WRITE(&pwm[0], pulse_width);
}

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

#define PWM_MAX_PERIOD 255  // 8 bits are used for our PWM period counter
#define PWM_MAX_DUTY_CYCLE PWM_MAX_PERIOD  // The max duty cycle is 100%, the same as the period
#define MODEL_CAR_PWM_MAX_VOLTAGE 3.3  // The car uses a 3.3 V connection.
#define MODEL_CAR_PWM_MIN_VOLTAGE 0    // The entire connection is used.
#define MODEL_CAR_MAX_SPEED 100 // This is what we define the highest possible acceleration
                                // value to be, and thus the highest speed we give to the car.
                                // This is a linear mapping under PWM but it is unlikely
                                // to be an accurate number. That is, a speed of 100 is unlikely
                                // to actually drive the car at 100 mph.
#define MODEL_CAR_MIN_SPEED 0

void pwm_signal_car() {
    volatile pwm_t *pwm = MMIO_CAPABILITY(pwm_t, pwm);
    uint32_t car_speed = MIN(car_info.acceleration, MODEL_CAR_MAX_SPEED);
    car_speed = MAX(car_speed, MODEL_CAR_MIN_SPEED);
    uint32_t pwm_duty_cycle = (car_speed * PWM_MAX_DUTY_CYCLE) / MODEL_CAR_MAX_SPEED;
    set_pwm(PWM_FROM_ADDR_AND_INDEX(pwm, 0), 
            PWM_MAX_PERIOD, 
            pwm_duty_cycle);
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
        //update_speed_estimate(); // TODO remove when actual car is in
        receive_ethernet_frame();
        pwm_signal_car();
        Debug::log("Current pedal info - acceleration={}, braking={}", (int) car_info.acceleration, (int) car_info.braking);
        char accelerationStr[50];
        memcpy(accelerationStr, "Speed: ", 7);
        size_t_to_str_base10(&accelerationStr[7], (size_t) car_info.acceleration, 0, 10);
        char brakingStr[50];
        memcpy(brakingStr, "Braking: ", 9);
        size_t_to_str_base10(&brakingStr[9], (size_t) car_info.braking, 0, 10);
        //char speedStr[50];
        //memcpy(speedStr, "Estimated Speed: ", 17);
        //size_t_to_str_base10(&speedStr[17], (size_t) car_info.speed, 0, 10);
        lcd->draw_str({centre.x - 55, centre.y - 40}, "Press the joystick to reset!", 
            Color::Black, Color::White);
        lcd->draw_str({centre.x - 48, centre.y - 10}, accelerationStr, Color::Black, 
            (car_info.acceleration > 100) ? Color::Red : Color::White);
        lcd->draw_str({centre.x - 48, centre.y}, brakingStr, Color::Black, 
            Color::White);
        //lcd->draw_str({centre.x - 48, centre.y + 10}, speedStr, Color::Black, 
        //    (car_info.acceleration > 100) ? Color::Red : Color::White);

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
