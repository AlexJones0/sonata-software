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

typedef enum LCDColor {
    ColorBlack = 0x000000,
    ColorWhite = 0xFFFFFF,
    ColorGrey  = 0xCCCCCC,
    ColorRed   = 0x0000FF,
} LCDColor;

typedef enum LCDFont {
    M3x6_16pt,
    LucidaConsole_10pt,
    LucidaConsole_12pt,
} LCDFont;

typedef struct EthernetHeader {
    uint8_t mac_destination[6];
    uint8_t mac_source[6];
    uint8_t type[2];
} __attribute__((__packed__)) EthernetHeader;

typedef enum DemoMode {
    DemoModePassthrough = 0,
    DemoModeSimulated = 1,
} DemoMode;

typedef enum FrameType {
    FrameDemoMode = 0,
    FramePedalData = 1,
} FrameType;

typedef struct DemoFrame {
    EthernetHeader header;
    FrameType type;
    union {
        DemoMode mode;
        uint8_t pedalData[16];
    } data;
} DemoFrame;

typedef struct {
    uint32_t x;
    uint32_t y;
} LCD_Size;

typedef struct LCD_Callbacks {
    void (*draw_str)(uint32_t x, uint32_t y, LCDFont font, const char *format, uint32_t bg_color, uint32_t fg_color, ...);
    void (*clean)(uint32_t color);
    void (*fill_rect)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
    void (*draw_img_rgb565)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data);
} LCD_Callbacks;

typedef struct Automotive_Callbacks {
    void (*uart_send)(const char *__restrict__ __format, ...);
    uint64_t (*wait)(const uint64_t wait_for);
    uint64_t wait_time;
    uint64_t (*time)();
    void (*loop)(void);
    void (*start)(void);
    uint8_t (*joystick_read)(void);
    bool (*digital_pedal_read)(void);
    uint32_t (*analogue_pedal_read)(void); 
    void (*ethernet_transmit)(const uint8_t *buffer, uint16_t length);
    LCD_Callbacks lcd;
} Automotive_Callbacks;


typedef struct TaskOne {
    uint64_t acceleration;
    uint64_t braking;
    uint64_t speed;
} TaskOne;

typedef struct TaskTwo {
    uint64_t write[100];
} TaskTwo;


extern LCD_Size lcdSize, lcdCentre;
extern Automotive_Callbacks callbacks;
extern const EthernetHeader FixedDemoHeader;


#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
void init_lcd(uint32_t size_x, uint32_t size_y);
void init_callbacks(Automotive_Callbacks auto_callbacks);
bool joystick_in_direction(uint8_t joystick, enum JoystickDir direction);
void send_data_frame(const uint64_t *data, EthernetHeader header, uint16_t length);
void send_mode_frame(EthernetHeader header, DemoMode mode);
#ifdef __cplusplus
}
#endif //__cplusplus

#endif // AUTOMOTIVE_COMMON_H
