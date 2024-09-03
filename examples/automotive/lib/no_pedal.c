// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include "no_pedal.h"


static TaskOne *task_one_mem;
static TaskTwo *task_two_mem;

static bool first_call_to_task = true;


void init_no_pedal_demo_mem(TaskOne *task_one, TaskTwo *task_two) {
    task_one_mem = task_one;
    task_two_mem = task_two;
}

static bool task_one() {
    callbacks.uart_send("Sending pedal data: acceleration=%u, braking=%u.\n", 
        (unsigned int) task_one_mem->acceleration, 
        (unsigned int) task_one_mem->braking);
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
bool task_two() {
    static uint64_t counter;
    if (first_call_to_task) {
        counter = 0;
        first_call_to_task = false;
    }
    counter += 1;
    callbacks.uart_send("Calling task two update, count = %u\n", (unsigned int) counter);
    callbacks.lcd.draw_str(10, 20, M3x6_16pt, "counter = %u;", ColorBlack, ColorGrey, (unsigned int) counter);
    callbacks.lcd.draw_str(10, 30, M3x6_16pt, "if (counter <= 100) {", ColorBlack, ColorGrey);
    uint32_t text_color = (counter >= 100) ? ColorRed : ColorGrey;
    callbacks.lcd.draw_str(10, 40, M3x6_16pt, "    write[counter] = 1000;", ColorBlack, text_color);
    callbacks.lcd.draw_str(10, 50, M3x6_16pt, "}", ColorBlack, ColorGrey);
    if (counter == 100) {
        callbacks.uart_send("counter == 100: triggering a bug\n");
    }
    if (counter <= 100) {
        task_two_mem->write[counter] = 1000;
    }
    return true;
}

void run_no_pedal_demo(uint64_t init_time)
{
    callbacks.start();

    send_mode_frame(FixedDemoHeader, DemoModePassthrough);

    task_one_mem->acceleration = 15;
    task_one_mem->braking = 0;
    task_one_mem->speed = 0;
    first_call_to_task = true;

    callbacks.uart_send("Automotive demo started!\n");
    callbacks.lcd.draw_str(10, 10, M3x6_16pt, "uint64_t write[100];", ColorBlack, ColorGrey);
    uint64_t last_elapsed_time = init_time;
    for (uint32_t i = 0; i < 175; i++) {
        task_one();
        task_two();
        last_elapsed_time = callbacks.wait(last_elapsed_time + callbacks.wait_time);
        callbacks.loop();
    }
    callbacks.uart_send("Automotive demo ended!\n");
}
