// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <platform-ethernet.hh>

#include "../../libraries/lcd.hh"

#include "./lib/automotive_common.h"

using Debug = ConditionalDebug<true, "Automotive-Receive">;
using namespace CHERI;
using namespace sonata::lcd;

// PWM constants
#define PWM_MAX_PERIOD 255                // 8 bits are used for our PWM period counter
#define PWM_MAX_DUTY_CYCLE PWM_MAX_PERIOD // The max duty cycle is 100%
#define PWM_MIN_DUTY_CYCLE 20             // For our car, at least 25/255 duty cycle is needed
                                          // to drive the motor, so we set "zero" to be 20.

// Simulation constants
#define DELTA_TIME_MSEC 80 // How often the main loop is updated. This includes the
                           // simulated speed when applicable, writing to the display,
                           // and polling for (and receiving) Ethernet packets.
#define MODEL_CAR_MAX_SPEED 200 // This is what we define the highest possible acceleration
                                // value to be, and thus the highest speed we can give to the 
                                // car. This is a linear mapping under PWM but it is unlikely
                                // to be an accurate number. That is, a speed of 100 will not
                                // actually drive the car at 100 mph.
#define MODEL_CAR_MIN_SPEED 0 
#define MODEL_CAR_ENGINE_HORSEPOWER 500 // Arbitrary numbers that feel good, not realistic
#define MODEL_CAR_BRAKING_FORCE MODEL_CAR_ENGINE_HORSEPOWER
#define MODEL_CAR_AIR_DENSITY 1
#define MODEL_CAR_DRAG_COEFFICIENT 1
#define MODEL_CAR_REFERENCE_AREA 5
#define MODEL_CAR_FRICTION_COEFFICIENT 40
#define SIM_DIVIDER 1000 // We implement fixed-point arithmetic by accumulating our 
                         // accelerating/decelerating forces and then make integer changes
                         // when a multiple of this divider has been accumulated. We use 
                         // 1000 for 1000 Msec, for use with DELTA_TIME_MSEC.

// Macros for cleanly reading from and writing to MMIO locations
#define MMIO_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = val)

// Macro for getting the MMIO location of a given PWM based on the
// PWM base address and the PWM index.
#define PWM_FROM_ADDR_AND_INDEX(addr, index) (&(((pwm_t)addr)[2 * index]))

EthernetDevice *ethernet;
SonataLcd *lcd;

struct CarInfo {
    uint64_t acceleration;
    uint64_t braking;
    uint64_t speed;
} car_info;

bool flag_reset = false;
uint8_t operating_mode = 0;

typedef uint32_t* pwm_t;

void set_pwm(pwm_t pwm, uint32_t counter, uint32_t pulse_width) {
  MMIO_WRITE(&pwm[1], counter);
  MMIO_WRITE(&pwm[0], pulse_width);
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
    DemoFrame frame;
    uint32_t index = 0;
    for (; index < 14; index++) {
        frame.header.mac_destination[index] = maybeFrame->buffer[index];
    }
    frame.type = static_cast<FrameType>(maybeFrame->buffer[index++]);
    switch (frame.type) {
        case FrameDemoMode:
            frame.data.mode = static_cast<DemoMode>(maybeFrame->buffer[index]);
            operating_mode = frame.data.mode;
            Debug::log("Received a mode frame with mode {}",
                (unsigned int) operating_mode);
            break;
        case FramePedalData:
            for (uint32_t i = 0; i < 16; ++i) {
                frame.data.pedalData[i] = maybeFrame->buffer[index++];
            }
            car_info.acceleration = 0x0;
            car_info.braking = 0x0;
            for (uint32_t i = 0; i < 8; ++i) {
                uint32_t shift = (7 - i) * 8;
                car_info.acceleration |= ((uint64_t) frame.data.pedalData[i]) << shift;
                car_info.braking |= ((uint64_t) frame.data.pedalData[i+8]) << shift;
            }
            Debug::log("Received a pedal data frame with acceleration {}",
                (unsigned int) car_info.acceleration);
            break;
        default: 
            Debug::log("Error: Unknown frame type!");
            return;
    }
}

void pwm_signal_car() {
    volatile pwm_t *pwm = MMIO_CAPABILITY(pwm_t, pwm);
    // Clamp the car speed between its min and max values.
    uint32_t car_speed = MIN(car_info.speed, MODEL_CAR_MAX_SPEED);
    car_speed = MAX(car_speed, MODEL_CAR_MIN_SPEED);
    // Linearly transform the car's speed to the same proportional
    // duty cycle representation in the valid duty cycle range.
    constexpr uint32_t DUTY_RANGE = PWM_MAX_DUTY_CYCLE - PWM_MIN_DUTY_CYCLE;
    constexpr uint32_t SPEED_RANGE = MODEL_CAR_MAX_SPEED - MODEL_CAR_MIN_SPEED;
    uint32_t speed_offset = car_speed - MODEL_CAR_MIN_SPEED;
    // We must * DUTY_RANGE before we / SPEED_RANGE as we don't have floats
    uint32_t pwm_duty_cycle = (speed_offset * DUTY_RANGE) / SPEED_RANGE + PWM_MIN_DUTY_CYCLE;
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
    static uint32_t partial_accel_accum;
    static uint32_t partial_decel_accum;

#ifdef SIM_DEBUG_PRINT
    Debug::log("SimulationPartialAccumDivider: {}", SIM_DIVIDER);
    Debug::log("DeltaTimeMsec: {}", DELTA_TIME_MSEC);
    Debug::log("Horsepower: {}", MODEL_CAR_ENGINE_HORSEPOWER);
    Debug::log("Braking force: {}", MODEL_CAR_BRAKING_FORCE);
    Debug::log("AirDensity: {}", MODEL_CAR_AIR_DENSITY);
    Debug::log("DragCoefficient: {}", MODEL_CAR_DRAG_COEFFICIENT);
    Debug::log("ReferenceArea: {}", MODEL_CAR_REFERENCE_AREA);
    Debug::log("Acceleration: {}", (int) car_info.acceleration);
    Debug::log("CarSpeed: {}", (int) car_info.speed);
    Debug::log("PartialAccelAccum: {}", (int) partial_accel_accum);
    Debug::log("PartialDecelAccum: {}", (int) partial_decel_accum);
    Debug::log("---");
#endif //SIM_DEBUG_PRINT

    partial_accel_accum += (MODEL_CAR_ENGINE_HORSEPOWER * car_info.acceleration) / DELTA_TIME_MSEC;
    partial_decel_accum += (MODEL_CAR_AIR_DENSITY * MODEL_CAR_DRAG_COEFFICIENT * MODEL_CAR_REFERENCE_AREA * car_info.speed * car_info.speed) / DELTA_TIME_MSEC;
    partial_decel_accum += (MODEL_CAR_BRAKING_FORCE * car_info.braking) / DELTA_TIME_MSEC;
    if (partial_accel_accum > SIM_DIVIDER) {
        car_info.speed += partial_accel_accum / SIM_DIVIDER;
        partial_accel_accum = partial_accel_accum % SIM_DIVIDER;
    }
    if (partial_decel_accum > SIM_DIVIDER) {
        uint32_t decrease = partial_decel_accum / SIM_DIVIDER;
        if (decrease > car_info.speed) {
            car_info.speed = 0;
        } else {
            car_info.speed -= decrease;
        }
        partial_decel_accum = partial_decel_accum % SIM_DIVIDER;
    }
    if (car_info.speed == 0 && car_info.acceleration == 0) {
        partial_accel_accum = 0;
        partial_decel_accum = 0;
    }
}

// Thread entry point.
[[noreturn]] void __cheri_compartment("automotive_receive") entry() {
    // Initialise ethernet driver for use via callback
    ethernet = new EthernetDevice();
    ethernet->mac_address_set({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB});
    
#ifdef AUTOMOTIVE_WAIT_FOR_ETHERNET
    while (!ethernet->phy_link_status()) {
        thread_millisecond_wait(50);
    }
#endif // AUTOMOTIVE_WAIT_FOR_ETHERNET
    
    lcd = new SonataLcd();
    Size  displaySize = lcd->resolution();
    Point centre      = {displaySize.width / 2, displaySize.height / 2};
    lcd->clean(Color::Black);
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);

    const uint32_t cyclesPerMillisecond = CPU_TIMER_HZ / 1000;
    const uint32_t waitTime = DELTA_TIME_MSEC * cyclesPerMillisecond;
    uint64_t prev_time = rdcycle64();
    while (true) {
        receive_ethernet_frame();
        pwm_signal_car();
        if (operating_mode == 0) {
            car_info.speed = car_info.acceleration;
            Debug::log("Current pedal info - acceleration={}, braking={}", (int) car_info.acceleration, (int) car_info.braking);
            char accelerationStr[50];
            memcpy(accelerationStr, "Speed: ", 7);
            size_t_to_str_base10(&accelerationStr[7], (size_t) car_info.acceleration, 0, 10);
            char brakingStr[50];
            memcpy(brakingStr, "Braking: ", 9);
            size_t_to_str_base10(&brakingStr[9], (size_t) car_info.braking, 0, 10);
            lcd->draw_str({centre.x - 55, centre.y - 40}, "Press the joystick to reset!", 
                Color::Black, Color::White);
            lcd->draw_str({centre.x - 48, centre.y - 10}, accelerationStr, Color::Black, 
                (car_info.acceleration > 80) ? Color::Red : Color::White);
            lcd->draw_str({centre.x - 48, centre.y}, brakingStr, Color::Black, 
                Color::White);
        } else {
            update_speed_estimate();
            char accelerationStr[50];
            memcpy(accelerationStr, "Acceleration: ", 14);
            size_t_to_str_base10(&accelerationStr[14], (size_t) car_info.acceleration, 0, 10);
            char speedStr[50];
            memcpy(speedStr, "Estimated Speed: ", 17);
            size_t_to_str_base10(&speedStr[17], (size_t) car_info.speed, 0, 10);
            lcd->draw_str({centre.x - 55, centre.y - 40}, "Press the joystick to reset!", 
                Color::Black, Color::White);
            lcd->draw_str({centre.x - 48, centre.y - 10}, accelerationStr, Color::Black, 
                (car_info.acceleration > 100) ? Color::Red : Color::White);
            Color speedColor = Color::White;
            if (65 < car_info.speed & car_info.speed < 75) {
                speedColor = Color::Green;
            } else if (car_info.speed >= 75) {
                speedColor = Color::Red;
            }
            lcd->draw_str({centre.x - 48, centre.y}, speedStr, Color::Black, 
                speedColor);
        }
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
