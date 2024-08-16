// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "automotive_common.h"

uint64_t wait_time;
LCD_Size lcdSize, lcdCentre;

void (*uart_callback)(const char *__restrict__ __format, ...);
uint64_t (*wait_callback)(const uint64_t wait_for);
uint64_t (*time_callback)();
void (*loop_callback)(void);
void (*start_callback)(void);
uint8_t (*joystick_read_callback)(void);
// TODO translate this into just a struct of LCD callbacks so this is cleaner & more extensible?
void (*lcd_draw_str_callback)(uint32_t x, uint32_t y, const char *format, uint32_t bg_color, uint32_t fg_color, ...);
void (*lcd_clean_callback)(uint32_t color);
void (*lcd_fill_rect_callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void (*lcd_draw_img_callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data);
void (*ethernet_transmit_callback)(const uint8_t *buffer, uint16_t length);

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

void init_ethernet_transmit_callback(void (*callback)(const uint8_t *buffer, uint16_t length)) {
    ethernet_transmit_callback = callback;
}

bool joystick_in_direction(uint8_t joystick, enum JoystickDir direction) {
    return (joystick & ((uint8_t) direction)) > 0;
};

void send_frame(const uint64_t *data, EthernetHeader header, uint16_t length) {
    assert(length < (100 / 8));  // TODO could up length to about 1000 bytes?
    uint8_t frame_buf[128];
    uint8_t frame_len = 0;
    for (uint8_t i = 0; i < 14; ++i) {
        frame_buf[frame_len++] = header.mac_destination[i];
    }
    for (uint8_t i = 0; i < length; ++i) {
        for (int8_t j = 7; j >= 0; --j) {
            frame_buf[frame_len++] = (data[i] >> (8 * j)) & 0xFF;
        }
    }
    ethernet_transmit_callback(frame_buf, frame_len);
}
