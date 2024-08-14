// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>
#include "./cursor.h"

typedef struct TaskOne {
    uint64_t acceleration;
    uint64_t braking;
    uint64_t speed;
} TaskOne;

typedef struct TaskTwo {
    uint64_t write[100];
} TaskTwo;

enum JoystickDir {
	Left    = 1 << 0,
	Up      = 1 << 1,
	Pressed = 1 << 2,
	Down    = 1 << 3,
	Right   = 1 << 4,
};

static TaskOne *task_one_mem;
static TaskTwo *task_two_mem;

static bool first_call = true;
static uint64_t wait_time;
static struct {
    uint32_t x;
    uint32_t y;
} lcdSize, lcdCentre;

static void (*uart_callback)(const char *__restrict__ __format, ...);
static uint64_t (*wait_callback)(const uint64_t wait_for);
static uint64_t (*time_callback)();
static void (*loop_callback)(void);
static void (*start_callback)(void);
static uint8_t (*joystick_read_callback)(void);
// TODO translate this into just a struct of LCD callbacks so this is cleaner & more extensible?
static void (*lcd_draw_str_callback)(uint32_t x, uint32_t y, const char *format, uint32_t bg_color, uint32_t fg_color, ...);
static void (*lcd_clean_callback)(uint32_t color);
static void (*lcd_fill_rect_callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
static void (*lcd_draw_img_callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data);
static void (*ethernet_transmit_callback)(const uint64_t *buffer, uint16_t length);

void init_mem(TaskOne *task_one, TaskTwo *task_two) {
    task_one_mem = task_one;
    task_two_mem = task_two;
}

void init_uart_callback(void (*callback)(const char *__restrict__ __format, ...)) {
    uart_callback = callback;
}

void init_wait_callback(uint64_t duration, uint64_t (*callback)(const uint64_t wait_for)) {
    wait_time = duration;
    wait_callback = callback;
}

void init_time_callback(uint64_t (*callback)()) {
    time_callback = callback;
}

void init_loop_callback(void (*callback)(void)) {
    loop_callback = callback;
}

void init_start_callback(void (*callback)(void)) {
    start_callback = callback;
}

void init_joystick_read_callback(uint8_t (callback)(void)) {
    joystick_read_callback = callback;
}

void init_lcd(uint32_t size_x, uint32_t size_y) {
    lcdSize.x = size_x;
    lcdSize.y = size_y;
    lcdCentre.x = size_x / 2;
    lcdCentre.y = size_y / 2;
}

void init_lcd_draw_str_callback(void (*callback)(uint32_t x, uint32_t y, const char *format, uint32_t bg_color, uint32_t fg_color, ...)) {
    lcd_draw_str_callback = callback;
}

void init_lcd_clean_callback(void (*callback)(uint32_t color)) {
    lcd_clean_callback = callback;
}

void init_lcd_fill_rect_callback(void (*callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)) {
    lcd_fill_rect_callback = callback;
}

void init_lcd_draw_img_callback(void (*callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data)) {
    lcd_draw_img_callback = callback;
}

void init_ethernet_transmit_callback(void (*callback)(const uint64_t *buffer, uint16_t length)) {
    ethernet_transmit_callback = callback;
}

void fill_option_select_rects(uint8_t prev, uint8_t current, bool cursor_img) {
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

bool joystick_in_direction(uint8_t joystick,
                           enum JoystickDir direction)
{
    return (joystick & (uint8_t) (direction)) > 0;
};

uint8_t select_demo() {
    lcd_clean_callback(0x000000);
    lcd_draw_str_callback(lcdCentre.x - 45, lcdCentre.y - 30, "Select Demo Application", 0x000000, 0xFFFFFF);
    lcd_draw_str_callback(lcdCentre.x - 60, lcdCentre.y - 10, "[1] Trigger simple bug", 0x000000, 0xFFFFFF);
    lcd_draw_str_callback(lcdCentre.x - 60, lcdCentre.y + 5, "[2] Other unimplemented option", 0x000000, 0xFFFFFF);
    uint8_t prev_option = 0, current_option = 0;
    bool cursor_img = true;
    fill_option_select_rects(prev_option, current_option, cursor_img);
    bool option_selected = false;
    uint64_t last_time = time_callback();
    uart_callback("Waiting for user input in the main menu...");
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

bool task_one() {
    uart_callback("Sending pedal data: acceleration=%u, braking=%u.\n", 
        (unsigned int) task_one_mem->acceleration, 
        (unsigned int) task_one_mem->braking);
    const uint64_t transmit_buf[2] = {
        task_one_mem->acceleration, 
        task_one_mem->braking
    };
    ethernet_transmit_callback(transmit_buf, 2);
    return true;
}

bool task_two() {
    static uint64_t counter;
    if (first_call) {
        counter = 0;
        first_call = false;
    }
    counter += 1;
    uart_callback("Calling task two update, count = %u\n", (unsigned int) counter);
    lcd_draw_str_callback(10, 20, "counter = %u;", 0x000000, 0xCCCCCC, (unsigned int) counter);
    lcd_draw_str_callback(10, 30, "if (counter <= 100) {", 0x000000, 0xCCCCCC);
    uint32_t text_color = (counter >= 100) ? 0x0000FF : 0xCCCCCC;
    lcd_draw_str_callback(10, 40, "    write[counter] = 1000;", 0x000000, text_color);
    lcd_draw_str_callback(10, 50, "}", 0x000000, 0xCCCCCC);
    if (counter == 100) {
        uart_callback("counter == 100: triggering a bug\n");
    }
    if (counter <= 100) {
        task_two_mem->write[counter] = 1000;
    }
    return true;
}

void run(uint64_t init_time)
{
    // TODO replace with proper zero-ing when pedal added
    start_callback();
    task_one_mem->acceleration = 12;
    task_one_mem->braking = 2;
    task_one_mem->speed = 0;
    first_call = true;

	uart_callback("Automotive demo started!\n");
    lcd_draw_str_callback(10, 10, "uint64_t write[100];", 0x000000, 0xCCCCCC);
    uint64_t last_elapsed_time = init_time;
    for (uint32_t i = 0; i < 175; i++) {
        task_one();
        task_two();
        last_elapsed_time = wait_callback(last_elapsed_time + wait_time);
        loop_callback();
    }
    uart_callback("Automotive demo ended!\n");
}
