// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include "joystick_pedal.h"
#include "automotive_common.h"

static TaskOne *task_one_mem;
static TaskTwo *task_two_mem;

static bool isBugged = false;
static uint64_t bugTime = 0;

void init_joystick_demo_mem(TaskOne *task_one, TaskTwo *task_two) {
    task_one_mem = task_one;
    task_two_mem = task_two;
}

static bool joystick_task_one() {
    callbacks.uart_send("Sending pedal data: acceleration=%u, braking=%u.\n", 
        (unsigned int) task_one_mem->acceleration, 
        (unsigned int) task_one_mem->braking);
    callbacks.lcd.draw_str(10, 50, M3x6_16pt, "Current speed: %u   ", ColorBlack, ColorGrey,
        (unsigned int) task_one_mem->speed);
    callbacks.lcd.draw_str(10, 60, M3x6_16pt, "Joystick up/down to change speed", ColorBlack, ColorGrey);
    uint8_t joystick = callbacks.joystick_read();
    if (joystick_in_direction(joystick, Right) && task_one_mem->speed < 99) {
        task_one_mem->speed += 1;
        task_one_mem->acceleration += 1;
    } else if (joystick_in_direction(joystick, Left) && task_one_mem->speed > 0) {
        task_one_mem->speed -= 1;
        task_one_mem->acceleration -= 1;
    }
    if (callbacks.time() > (bugTime + 3 * callbacks.wait_time) && (joystick_in_direction(joystick, Up) | joystick_in_direction(joystick, Down))) {
        isBugged = !isBugged;
        bugTime = callbacks.time();
        task_one_mem->acceleration = task_one_mem->speed;
        callbacks.uart_send("Manually triggering/untriggering bug.");
    }
    const uint64_t frame_data[2] = {
        task_one_mem->acceleration, 
        task_one_mem->braking
    };
    send_data_frame(frame_data, FixedDemoHeader, 2);
    return true;
}

// Even though we this function is not exported, do not declare this function 
// as static, as otherwise the PCC modifying trick used to catch the CHERI
// exceptions without compartmentalising will not work correctly.
bool joystick_task_two() {
    volatile uint32_t count = 99;
    if (!isBugged) {
        callbacks.lcd.draw_str(10, 20, M3x6_16pt, "Not triggered", ColorBlack, ColorGrey);
        callbacks.lcd.draw_str(10, 30, M3x6_16pt, "Joystick left/right to trigger", ColorBlack, ColorGrey);
    } else {
        count = 100;
        callbacks.lcd.draw_str(10, 20, M3x6_16pt, "Bug triggered", ColorBlack, ColorGrey);
    }
    if (count <= 100) {
        task_two_mem->write[count] = 1000;
    }
    return true;
}

void run_joystick_demo(uint64_t init_time)
{
    callbacks.start();

    send_mode_frame(FixedDemoHeader, DemoModePassthrough);

    isBugged = false;
    task_one_mem->acceleration = 15;
    task_one_mem->braking = 0;
    task_one_mem->speed = task_one_mem->acceleration;

	callbacks.uart_send("Automotive demo started!\n");
    uint64_t last_elapsed_time = init_time;
    bool stillRunning = true;
    while (stillRunning) {
        joystick_task_one();
        joystick_task_two();
        callbacks.lcd.draw_str(10, 80, M3x6_16pt, "Press the joystick to end the demo.", ColorBlack, ColorGrey);
        if (last_elapsed_time > (init_time + callbacks.wait_time * 5) && joystick_in_direction(callbacks.joystick_read(), Pressed)) {
            stillRunning = false;
            callbacks.uart_send("Manually ended joystick demo by pressing joystick.");
        }
        last_elapsed_time = callbacks.wait(last_elapsed_time + callbacks.wait_time);
        callbacks.loop();
    }
    callbacks.uart_send("Automotive demo ended!\n");
}
