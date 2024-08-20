// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include "analogue_pedal.h"
#include "automotive_common.h"

static TaskOne *task_one_mem;
static TaskTwo *task_two_mem;

static bool isBugged = false;
static uint64_t bugTime = 0;

void init_analogue_pedal_demo_mem(TaskOne *task_one, TaskTwo *task_two) {
    task_one_mem = task_one;
    task_two_mem = task_two;
}

static bool analogue_task_one() {
    callbacks.uart_send("Sending pedal data: acceleration=%u, braking=%u.\n", 
        (unsigned int) task_one_mem->acceleration, 
        (unsigned int) task_one_mem->braking);
    uint8_t joystick = callbacks.joystick_read();
    if (callbacks.time() > (bugTime + 3 * callbacks.wait_time) && (joystick_in_direction(joystick, Up) | joystick_in_direction(joystick, Down))) {
        isBugged = !isBugged;
        bugTime = callbacks.time();
        callbacks.uart_send("Manually triggering/untriggering bug.");
    }
    const uint64_t frame_data[2] = {
        task_one_mem->acceleration, 
        task_one_mem->braking
    };
    send_data_frame(frame_data, FixedDemoHeader, 2);
    task_one_mem->acceleration = callbacks.analogue_pedal_read();
    return true;
}

// Even though we this function is not exported, do not declare this function 
// as static, as otherwise the PCC modifying trick used to catch the CHERI
// exceptions without compartmentalising will not work correctly.
bool analogue_task_two() {
    volatile uint32_t count = 99;
    if (!isBugged) {
        callbacks.lcd.draw_str(10, 20, "Not triggered", ColorBlack, ColorGrey);
        callbacks.lcd.draw_str(10, 30, "Joystick left/right to trigger", ColorBlack, ColorGrey);
    } else {
        count = 100;
        callbacks.lcd.draw_str(10, 20, "Bug triggered", ColorBlack, ColorGrey);
    }
    if (count <= 100) {
        task_two_mem->write[count] = 1000;
    }
    return true;
}

void run_analogue_pedal_demo(uint64_t init_time)
{
    callbacks.start();

    send_mode_frame(FixedDemoHeader, DemoModePassthrough);

    isBugged = false;
    task_one_mem->acceleration = 0;
    task_one_mem->braking = 0;
    task_one_mem->speed = task_one_mem->acceleration;

	callbacks.uart_send("Automotive demo started!\n");
    uint64_t last_elapsed_time = init_time;
    bool stillRunning = true;
    while (stillRunning) {
        analogue_task_one();
        analogue_task_two();
        callbacks.lcd.draw_str(10, 80, "Press the joystick to end the demo.", ColorBlack, ColorGrey);
        if (last_elapsed_time > (init_time + callbacks.wait_time * 5) && joystick_in_direction(callbacks.joystick_read(), Pressed)) {
            stillRunning = false;
            callbacks.uart_send("Manually ended joystick demo by pressing joystick.");
        }
        last_elapsed_time = callbacks.wait(last_elapsed_time + callbacks.wait_time);
        callbacks.loop();
    }
    callbacks.uart_send("Automotive demo ended!\n");
}
