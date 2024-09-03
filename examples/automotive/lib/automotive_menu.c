// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>

#include "automotive_common.h"
#include "automotive_menu.h"
#include "./cursor.h"

static void fill_option_select_rects(uint8_t prev, uint8_t current, bool cursor_img) {
    callbacks.lcd.fill_rect(
        lcdCentre.x - 64, 
        lcdCentre.y - 22 + prev * 20,
        5, 
        5,
        ColorBlack
    );
    if (cursor_img) {
        callbacks.lcd.draw_img_rgb565(
            lcdCentre.x - 64,
            lcdCentre.y - 22 + current * 20,
            5,
            5,
            cursorImg5x5
        );
        return;
    }
    callbacks.lcd.fill_rect(
        lcdCentre.x - 64, 
        lcdCentre.y - 22 + current * 20,
        5, 
        5,
        ColorWhite
    );
}

DemoApplication select_demo() {
    callbacks.lcd.clean(ColorBlack);
    callbacks.lcd.draw_str(lcdCentre.x - 60, lcdCentre.y - 50, LucidaConsole_12pt, "Select Demo", ColorBlack, ColorWhite);
    callbacks.lcd.draw_str(lcdCentre.x - 55, lcdCentre.y - 25, LucidaConsole_10pt, "[1] Analogue", ColorBlack, 0x909090);
    callbacks.lcd.draw_str(lcdCentre.x - 55, lcdCentre.y - 5,  LucidaConsole_10pt, "[2] Digital", ColorBlack, 0x909090);
    callbacks.lcd.draw_str(lcdCentre.x - 55, lcdCentre.y + 15, LucidaConsole_10pt, "[3] Joystick", ColorBlack, 0x909090);
    callbacks.lcd.draw_str(lcdCentre.x - 55, lcdCentre.y + 35, LucidaConsole_10pt, "[4] No Pedal", ColorBlack, 0x909090);
    const uint8_t num_options = 4;
    uint8_t prev_option = 0, current_option = 0;
    bool cursor_img = true;
    fill_option_select_rects(prev_option, current_option, cursor_img);
    bool option_selected = false;
    uint64_t last_time = callbacks.time();
    uint64_t time_between_inputs = callbacks.wait_time * 2;
    callbacks.uart_send("Waiting for user input in the main menu...\n");
    while (!option_selected) {
        prev_option = current_option;
        uint8_t joystick_input = callbacks.joystick_read();
        if (joystick_in_direction(joystick_input, Right)) {
            if (callbacks.time() < last_time + time_between_inputs) {
                continue;
            } 
            current_option = (current_option == 0) ? (num_options - 1) : (current_option - 1);
            fill_option_select_rects(prev_option, current_option, cursor_img);
            last_time = callbacks.time();
        } else if (joystick_in_direction(joystick_input, Left)) {
            if (callbacks.time() < last_time + time_between_inputs) {
                continue;
            }
            current_option = (current_option + 1) % num_options;
            fill_option_select_rects(prev_option, current_option, cursor_img);
            last_time = callbacks.time();
        } else if (joystick_in_direction(joystick_input, Pressed)) {
            if (callbacks.time() < last_time + time_between_inputs) {
                continue;
            }
            option_selected = true;
            callbacks.lcd.clean(ColorBlack);
        }
    }
    return (DemoApplication) current_option;
}
