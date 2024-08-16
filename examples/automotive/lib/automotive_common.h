// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#ifndef AUTOMOTIVE_COMMON_H
#define AUTOMOTIVE_COMMON_H

#include <stdint.h>
#include <stdbool.h>

enum JoystickDir {
	Left    = 1 << 0,
	Up      = 1 << 1,
	Pressed = 1 << 2,
	Down    = 1 << 3,
	Right   = 1 << 4,
};

typedef struct EthernetHeader {
    uint8_t mac_destination[6];
    uint8_t mac_source[6];
    uint8_t type[2];
} __attribute__((__packed__)) EthernetHeader;

typedef struct {
    uint32_t x;
    uint32_t y;
} LCD_Size;

extern uint64_t wait_time;
extern LCD_Size lcdSize, lcdCentre;

extern void (*uart_callback)(const char *__restrict__ __format, ...);
extern uint64_t (*wait_callback)(const uint64_t wait_for);
extern uint64_t (*time_callback)();
extern void (*loop_callback)(void);
extern void (*start_callback)(void);
extern uint8_t (*joystick_read_callback)(void);
// TODO translate this into just a struct of LCD callbacks so this is cleaner & more extensible?
extern void (*lcd_draw_str_callback)(uint32_t x, uint32_t y, const char *format, uint32_t bg_color, uint32_t fg_color, ...);
extern void (*lcd_clean_callback)(uint32_t color);
extern void (*lcd_fill_rect_callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
extern void (*lcd_draw_img_callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data);
extern void (*ethernet_transmit_callback)(const uint8_t *buffer, uint16_t length);

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
void init_uart_callback(void (*callback)(const char *__restrict__ __format, ...));
void init_wait_callback(uint64_t duration, uint64_t (*callback)(const uint64_t wait_for));
void init_time_callback(uint64_t (*callback)());
void init_loop_callback(void (*callback)(void));
void init_start_callback(void (*callback)(void));
void init_joystick_read_callback(uint8_t (callback)(void));
void init_lcd(uint32_t size_x, uint32_t size_y);
void init_lcd_draw_str_callback(void (*callback)(uint32_t x, uint32_t y, const char *format, uint32_t bg_color, uint32_t fg_color, ...));
void init_lcd_clean_callback(void (*callback)(uint32_t color));
void init_lcd_fill_rect_callback(void (*callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color));
void init_lcd_draw_img_callback(void (*callback)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data));
void init_ethernet_transmit_callback(void (*callback)(const uint8_t *buffer, uint16_t length));
bool joystick_in_direction(uint8_t joystick, enum JoystickDir direction);
void send_frame(const uint64_t *data, EthernetHeader header, uint16_t length);
#ifdef __cplusplus
}
#endif //__cplusplus

#endif // AUTOMOTIVE_COMMON_H
