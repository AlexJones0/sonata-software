// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include "analogue_pedal.h"
#include "automotive_common.h"
#include "sound_icon.h"

static AnalogueTaskOne *task_one_mem;
static AnalogueTaskTwo *task_two_mem;

void init_analogue_pedal_demo_mem(AnalogueTaskOne *task_one, AnalogueTaskTwo *task_two) {
    task_one_mem = task_one;
    task_two_mem = task_two;
}

static uint64_t lerp_green_to_red(uint32_t portion, uint32_t segments) {
    // Simple linear interpolation, in reality we should do a spherical interpolation
    // in HSV colour space but the HSV->RGB logic is a bit more involved to implement,
    // so I'm leaving it at this for now.
    uint64_t red = 0x0000FF;
    uint64_t green = 0x00FF00;
    red *= portion;
    green *= (segments - portion);
    red /= segments;
    green /= segments;
    return (red & 0xFF) | (green & 0xFF00);
}

static void outline_volume_bar(uint32_t x, uint32_t y, uint32_t max_volume) {
    callbacks.lcd.fill_rect(
        x, y, 7 + max_volume * 6, 13, ColorWhite
    );
    callbacks.lcd.fill_rect(
        x+2, y+2, 3 + max_volume * 6, 9, ColorBlack
    );
}

static void draw_volume_bar(uint32_t x, uint32_t y, uint32_t max_volume) {
    for (uint32_t i = 0; i < max_volume; ++i) {
        callbacks.lcd.fill_rect(
            x + 4 + (i * 6),
            y + 4,
            5,
            5,
            task_two_mem->framebuffer[i]
        );
    }
}

static bool analogue_task_one() {
    callbacks.uart_send("Sending pedal data: acceleration=%u, braking=%u.\n", 
        (unsigned int) task_one_mem->acceleration, 
        (unsigned int) task_one_mem->braking);
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
    uint32_t prev_volume = task_two_mem->volume;
    uint8_t joystick = callbacks.joystick_read();
    if (joystick_in_direction(joystick, Up) && task_two_mem->volume > 0) {
        task_two_mem->volume -= 1;
        task_two_mem->framebuffer[task_two_mem->volume] = 0x000000;
    } else if (joystick_in_direction(joystick, Down) && task_two_mem->volume <= 20) {
        // Bugged line! Should be volume < 20, not volume <= 20.
        task_two_mem->volume += 1;
    }
    uint32_t str_color = (task_two_mem->volume > 20) ? ColorRed : ColorWhite;
    callbacks.lcd.draw_str(33, 30, LucidaConsole_10pt, "Volume: %u/%u ", ColorBlack, str_color, 
        (unsigned int) task_two_mem->volume, 20u);
    if (task_two_mem->volume != prev_volume) {
        callbacks.uart_send("Volume changed to %u.\n", (unsigned int) task_two_mem->volume);
        if (task_two_mem->volume == 21) {
            callbacks.uart_send("Bug triggered!\n");
       } 
    }
    if (task_two_mem->volume == 0) {
        return true;
    }
    task_two_mem->framebuffer[task_two_mem->volume-1] = lerp_green_to_red(task_two_mem->volume, 21);
    return true;
}

void run_analogue_pedal_demo(uint64_t init_time)
{
    callbacks.start();

    send_mode_frame(FixedDemoHeader, DemoModePassthrough);

    task_one_mem->acceleration = 0;
    task_one_mem->braking = 0;

    task_two_mem->volume = 15;
    outline_volume_bar(10, 45, 20);
    for (uint32_t i = 0; i < 20; ++i) {
        if (i < task_two_mem->volume) {
            task_two_mem->framebuffer[i] = lerp_green_to_red(i, 21);
        } else {
            task_two_mem->framebuffer[i] = 0x000000;
        }
    }
    callbacks.lcd.draw_img_rgb565(11, 30, 15, 11, soundIconImg15x11);

    callbacks.uart_send("Automotive demo started!\n");
    uint64_t last_elapsed_time = init_time;
    bool stillRunning = true;
    while (stillRunning) {
        analogue_task_one();
        analogue_task_two();
        draw_volume_bar(10, 45, 20);
        callbacks.lcd.draw_str(10, 60, LucidaConsole_10pt, "Exceed max volume", ColorBlack, 0xA0A0A0);
        callbacks.lcd.draw_str(10, 75, LucidaConsole_10pt, "for a bug!", ColorBlack, 0xA0A0A0);
        callbacks.lcd.draw_str(10, 12, M3x6_16pt, "Press the joystick to end the demo.", ColorBlack, 0x808080);
        if (last_elapsed_time > (init_time + callbacks.wait_time * 5) && joystick_in_direction(callbacks.joystick_read(), Pressed)) {
            stillRunning = false;
            callbacks.uart_send("Manually ended joystick demo by pressing joystick.");
        }
        last_elapsed_time = callbacks.wait(last_elapsed_time + callbacks.wait_time);
        callbacks.loop();
    }
    callbacks.uart_send("Automotive demo ended!\n");
}
