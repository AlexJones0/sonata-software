// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>

#include "automotive_common.h"
#include "automotive_menu.h"
#include "./cursor.h"

static void fill_option_select_rects(uint8_t prev, uint8_t current, bool cursor_img) {
    lcd_fill_rect_callback(
        lcdCentre.x - 70, 
        lcdCentre.y - 9 + prev * 15,
        5, 
        5,
        0x000000
    );
    if (cursor_img) {
        lcd_draw_img_callback(
            lcdCentre.x - 70,
            lcdCentre.y - 9 + current * 15,
            5,
            5,
            cursorImg5x5
        );
        return;
    }
    lcd_fill_rect_callback(
        lcdCentre.x - 70, 
        lcdCentre.y - 9 + current * 15,
        5, 
        5,
        0xFFFFFF
    );
}

uint8_t select_demo() {
    lcd_clean_callback(0x000000);
    lcd_draw_str_callback(lcdCentre.x - 45, lcdCentre.y - 30, "Select Demo Application", 0x000000, 0xFFFFFF);
    lcd_draw_str_callback(lcdCentre.x - 60, lcdCentre.y - 10, "[1] Simple (NO PEDAL)", 0x000000, 0xFFFFFF);
    lcd_draw_str_callback(lcdCentre.x - 60, lcdCentre.y + 5,  "[2] Simple (WITH PEDAL) [TODO]", 0x000000, 0xFFFFFF);
    uint8_t prev_option = 0, current_option = 0;
    bool cursor_img = true;
    fill_option_select_rects(prev_option, current_option, cursor_img);
    bool option_selected = false;
    uint64_t last_time = time_callback();
    uart_callback("Waiting for user input in the main menu...\n");
    while (!option_selected) {
        prev_option = current_option;
        uint8_t joystick_input = joystick_read_callback();
        if (joystick_in_direction(joystick_input, Left)) {
            if (time_callback() < last_time + wait_time * 3) {
                continue;
            } 
            current_option = (current_option == 0) ? 1 : (current_option - 1);
            fill_option_select_rects(prev_option, current_option, cursor_img);
            last_time = time_callback();
        } else if (joystick_in_direction(joystick_input, Right)) {
            if (time_callback() < last_time + wait_time * 3) {
                continue;
            }
            current_option = (current_option + 1) % 2;
            fill_option_select_rects(prev_option, current_option, cursor_img);
            last_time = time_callback();
        } else if (joystick_in_direction(joystick_input, Pressed)) {
            option_selected = true;
            lcd_clean_callback(0x000000);
        }
    }
    return current_option;
}
