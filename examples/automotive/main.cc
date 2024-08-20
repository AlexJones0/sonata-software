// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <platform-ethernet.hh>

#include "../../libraries/lcd.hh"
#include "../snake/cherry_bitmap.h"

#include "./lib/automotive_common.h"
#include "./lib/automotive_menu.h"
#include "./lib/no_pedal.h"
#include "./lib/joystick_pedal.h"
#include "./lib/digital_pedal.h"

using Debug = ConditionalDebug<true, "Automotive">;
using namespace CHERI;
using namespace sonata::lcd;

static bool error_seen = false;
static bool error_message_shown = false;

EthernetDevice *ethernet;
SonataLcd *lcd;

void size_t_to_str_base10(char *buffer, size_t num)
{
	// Parse the digits using repeated remainders mod 10
	ptrdiff_t endIdx = 0;
	if (num == 0)
	{
		buffer[endIdx++] = '0';
	}
	while (num != 0)
	{
		int remainder    = num % 10;
		buffer[endIdx++] = '0' + remainder;
		num /= 10;
	}
	buffer[endIdx--] = '\0';

	// Reverse the generated string
	ptrdiff_t startIdx = 0;
	while (startIdx < endIdx)
	{
		char swap          = buffer[startIdx];
		buffer[startIdx++] = buffer[endIdx];
		buffer[endIdx--]   = swap;
	}
}

void write_to_uart(const char *__format, ...) {
    va_list args;
    va_start(args, __format);
    char __buffer[1024];
    int strlen = 0;
    // Crudely replace %u manually
    const char *current_char = __format;
    while (*current_char != '\0') {
        if (current_char[0] == '%' && current_char[1] == 'u') {
            size_t argval = va_arg(args, unsigned int);
            size_t_to_str_base10(&__buffer[strlen], argval);
            while (__buffer[strlen] != '\0') {
                strlen++;
            }
            current_char++;
        } else {
            __buffer[strlen++] = *current_char;
        }
        current_char++;
    }
    __buffer[strlen] = '\0';
    if (__buffer[strlen-1] == '\n') {
        __buffer[--strlen] = '\0';
    } 
    va_end(args);

    Debug::log(__buffer, args);
}

uint64_t wait(const uint64_t wait_for) {
    uint64_t cur_time = rdcycle64();
    while (cur_time < wait_for) {
        cur_time = rdcycle64();
        // Busy wait, TODO could change to thread sleep?
    }
    return cur_time;
}

void reset_error_seen_and_shown(void) {
    error_seen = false;
    error_message_shown = false;
}

void lcd_display_cheri_message(void) {
    if (error_seen && !error_message_shown) {
        error_message_shown = true;
        Size  displaySize = lcd->resolution();
        Point centre      = {displaySize.width / 2, displaySize.height / 2};
        
        lcd->draw_str({centre.x - 70, centre.y + 27}, 
            "Unexpected CHERI capability violation!", 
            Color::Black, Color::Red);
        lcd->draw_str({centre.x - 65, centre.y + 40},
            "Memory has been safely protected",
            Color::Black, Color::Green);
        lcd->draw_str({centre.x - 20, centre.y + 50},
            "by CHERI.",
            Color::Black, Color::Green);
		lcd->draw_image_rgb565(
            Rect::from_point_and_size(
                {centre.x + 20, centre.y + 50},
                {10, 10}
            ), cherryImage10x10);
    }
}

void lcd_draw_str(uint32_t x, uint32_t y, const char *format, uint32_t bg_color, uint32_t fg_color, ...) {
    // TODO modularise this code, its repeated from the UART stuff, and a huge mess
    va_list args;
    va_start(args, fg_color);
    char __buffer[1024];
    int strlen = 0;
    // Crudely replace %u manually
    const char *current_char = format;
    while (*current_char != '\0') {
        if (current_char[0] == '%' && current_char[1] == 'u') {
            size_t argval = va_arg(args, unsigned int);
            size_t_to_str_base10(&__buffer[strlen], argval);
            while (__buffer[strlen] != '\0') {
                strlen++;
            }
            current_char++;
        } else {
            __buffer[strlen++] = *current_char;
        }
        current_char++;
    }
    __buffer[strlen] = '\0';
    if (__buffer[strlen-1] == '\n') {
        __buffer[--strlen] = '\0';
    } 
    va_end(args);

    lcd->draw_str({x, y}, __buffer, static_cast<Color>(bg_color), static_cast<Color>(fg_color));
}

void lcd_clean(uint32_t color) {
    lcd->clean(static_cast<Color>(color));
}

void lcd_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    lcd->fill_rect(Rect::from_point_and_size({x, y}, {w, h}), 
                   static_cast<Color>(color));
}

void lcd_draw_img(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint8_t *data) {
    lcd->draw_image_rgb565(Rect::from_point_and_size({x, y}, {w, h}), data);
}

uint8_t read_joystick(void) {
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);
    return static_cast<uint8_t>(gpio->read_joystick());
}

bool read_pedal_digital(void) {
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);
    return (gpio->input & (1 << 13)) > 0;
}

uint32_t read_pedal_analogue(void) {
    // TODO after implementing the ADC driver
    return 0;
}

void null_callback(void) {};

bool null_ethernet_callback(uint8_t *buffer, uint16_t length) { 
    return true; 
};

void send_ethernet_frame(const uint8_t *buffer, uint16_t length) {
    uint8_t *frame_buf = (uint8_t *) malloc(sizeof(uint8_t) * length);
    for (uint16_t i = 0; i < length; ++i) {
        frame_buf[i] = buffer[i];
    } 
    if (!ethernet->send_frame(frame_buf, length, null_ethernet_callback)) {
        Debug::log("Error sending frame...");
    }
    free(frame_buf);
}

static TaskTwo mem_task_two = {
	.write = {0},
};
static TaskOne mem_task_one = {
	.acceleration = 12,
	.braking = 2,
	.speed = 0,
};

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	auto [exceptionCode, registerNumber] = extract_cheri_mtval(mtval);
	if (exceptionCode == CauseCode::BoundsViolation ||
	    exceptionCode == CauseCode::TagViolation)
	{
        if (!error_seen) {
            // Two violations - one bound, one tag. Only show an error once
            error_seen = true;
            Debug::log("Unexpected CHERI capability violation!");
            Debug::log("Memory has been safely protected by CHERI.");
        }
        frame->pcc = (void *) ((uint32_t *) (frame->pcc) + 1);
        return ErrorRecoveryBehaviour::InstallContext;
	}

	Debug::log(
	  "Unexpected CHERI Capability violation encountered. Stopping...");
	return ErrorRecoveryBehaviour::ForceUnwind;
}

// Thread entry point.
void __cheri_compartment("automotive") entry() {
    
	lcd = new SonataLcd();
    Size  displaySize = lcd->resolution();
    Point centre      = {displaySize.width / 2, displaySize.height / 2};

    // Initialise ethernet driver for use via callback
    ethernet = new EthernetDevice();
    ethernet->mac_address_set();
    if (!ethernet->phy_link_status()) {
        Debug::log("Waiting for a good physical ethernet link...\n");
        lcd->clean(Color::Black);
        lcd->draw_str({centre.x - 55, centre.y - 5}, 
            "Waiting for a good physical", 
            Color::Black, Color::White);
        lcd->draw_str({centre.x - 30, centre.y + 5},
            "ethernet link...",
            Color::Black, Color::White);
    }
    while (!ethernet->phy_link_status()) {
        thread_millisecond_wait(50);
    }
    thread_millisecond_wait(250);

    // Adapt common automotive library for CHERIoT drivers
	const uint32_t cyclesPerMillisecond = CPU_TIMER_HZ / 1000;
    init_lcd(displaySize.width, displaySize.height);
    init_callbacks({
        .uart_send = write_to_uart,
        .wait = wait,
        .wait_time = 120 * cyclesPerMillisecond,
        .time = rdcycle64,
        .loop = lcd_display_cheri_message,
        .start = reset_error_seen_and_shown,
        .joystick_read = read_joystick,
        .digital_pedal_read = read_pedal_digital,
        .analogue_pedal_read = read_pedal_analogue,
        .ethernet_transmit = send_ethernet_frame,
        .lcd = {
            .draw_str = lcd_draw_str,
            .clean = lcd_clean,
            .fill_rect = lcd_fill_rect,
            .draw_img_rgb565 = lcd_draw_img,
        },
    });

    DemoApplication option;
    while (true) {
        // Run demo selection
        option = select_demo();

        // Run automotive demo
        switch (option) {
            case NoPedal:
                // Run simple timed demo with no pedal & using passthrough
                init_no_pedal_demo_mem(&mem_task_one, &mem_task_two);
                run_no_pedal_demo(rdcycle64());
                break;
            case JoystickPedal:
                // Run demo using a joystick as a pedal, with passthrough
                init_joystick_demo_mem(&mem_task_one, &mem_task_two);
                run_joystick_demo(rdcycle64());
                break;
            case DigitalPedal:
                // Run demo using an actual physical pedal but taking
                // a digital signal, using simulation instead of passthrough
                init_digital_pedal_demo_mem(&mem_task_one, &mem_task_two);
                run_digital_pedal_demo(rdcycle64());
                break;
            case AnaloguePedal:
                // TODO not implemented yet
                Debug::log("This demo is not yet implemented. It requires an ADC");
                // Run demo using an actual physical pedal, taking an
                // analogue signal via an ADC, with passthrough.
                break;

        }
    }

    // Cleanup
    delete lcd;
    delete ethernet;

}
