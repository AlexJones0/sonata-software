// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>

#include "automotive_common.h"
#include "automotive_menu.h"
#include "./cursor.h"

static void fill_option_select_rects(uint8_t prev, uint8_t current, bool cursor_img) {
    callbacks.lcd.fill_rect(
        lcdCentre.x - 70, 
        lcdCentre.y - 9 + prev * 15,
        5, 
        5,
        ColorBlack
    );
    if (cursor_img) {
        callbacks.lcd.draw_img_rgb565(
            lcdCentre.x - 70,
            lcdCentre.y - 9 + current * 15,
            5,
            5,
            cursorImg5x5
        );
        return;
    }
    callbacks.lcd.fill_rect(
        lcdCentre.x - 70, 
        lcdCentre.y - 9 + current * 15,
        5, 
        5,
        ColorWhite
    );
}

DemoApplication select_demo() {
    callbacks.lcd.clean(0x000000);
    callbacks.lcd.draw_str(lcdCentre.x - 45, lcdCentre.y - 30, "Select Demo Application", ColorBlack, ColorWhite);
    callbacks.lcd.draw_str(lcdCentre.x - 60, lcdCentre.y - 10, "[1] No pedal", ColorBlack, ColorWhite);
    callbacks.lcd.draw_str(lcdCentre.x - 60, lcdCentre.y + 5,  "[2] Joystick pedal passthrough", ColorBlack, ColorWhite);
    callbacks.lcd.draw_str(lcdCentre.x - 60, lcdCentre.y + 20, "[3] Digital pedal simulated", ColorBlack, ColorWhite);
    callbacks.lcd.draw_str(lcdCentre.x - 60, lcdCentre.y + 35, "[4] Analogue pedal passthrough", ColorBlack, ColorWhite);
    const uint8_t num_options = 4;
    uint8_t prev_option = 0, current_option = 0;
    bool cursor_img = true;
    fill_option_select_rects(prev_option, current_option, cursor_img);
    bool option_selected = false;
    uint64_t init_time = callbacks.time();
    uint64_t last_time = callbacks.time();
    callbacks.uart_send("Waiting for user input in the main menu...\n");
    while (!option_selected) {
        prev_option = current_option;
        uint8_t joystick_input = callbacks.joystick_read();
        if (joystick_in_direction(joystick_input, Right)) {
            if (callbacks.time() < last_time + callbacks.wait_time * 3) {
                continue;
            } 
            current_option = (current_option == 0) ? (num_options - 1) : (current_option - 1);
            fill_option_select_rects(prev_option, current_option, cursor_img);
            last_time = callbacks.time();
        } else if (joystick_in_direction(joystick_input, Left)) {
            if (callbacks.time() < last_time + callbacks.wait_time * 3) {
                continue;
            }
            current_option = (current_option + 1) % num_options;
            fill_option_select_rects(prev_option, current_option, cursor_img);
            last_time = callbacks.time();
        } else if (last_time > (init_time + callbacks.wait_time * 3) && joystick_in_direction(joystick_input, Pressed)) {
            option_selected = true;
            callbacks.lcd.clean(ColorBlack);
        }
    }
    return (DemoApplication) current_option;
}
