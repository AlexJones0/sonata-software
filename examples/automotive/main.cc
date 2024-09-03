// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <platform-adc.hh>
#include <platform-ethernet.hh>
#include <thread.h>

#include "../../libraries/lcd.hh"
#include "../snake/cherry_bitmap.h"

#include "./lib/analogue_pedal.h"
#include "./lib/automotive_common.h"
#include "./lib/automotive_menu.h"
#include "./lib/digital_pedal.h"
#include "./lib/joystick_pedal.h"
#include "./lib/no_pedal.h"

#define PEDAL_MIN_ANALOGUE 310
#define PEDAL_MAX_ANALOGUE 1700

using Debug = ConditionalDebug<true, "Automotive">;
using SonataAdc = SonataAnalogueDigitalConverter;
using namespace CHERI;
using namespace sonata::lcd;

static bool errorSeen         = false;
static bool errorMessageShown = false;

EthernetDevice *ethernet;
SonataAdc      *adc;
SonataLcd      *lcd;

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

void write_to_uart(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char __buffer[1024];
    int  strlen = 0;
    // Crudely replace %u manually
    const char *currentChar = format;
    while (*currentChar != '\0')
    {
        if (currentChar[0] == '%' && currentChar[1] == 'u')
        {
            size_t argval = va_arg(args, unsigned int);
            size_t_to_str_base10(&__buffer[strlen], argval);
            while (__buffer[strlen] != '\0')
            {
                strlen++;
            }
            currentChar++;
        }
        else
        {
            __buffer[strlen++] = *currentChar;
        }
        currentChar++;
    }
    __buffer[strlen] = '\0';
    if (__buffer[strlen - 1] == '\n')
    {
        __buffer[--strlen] = '\0';
    }
    va_end(args);

    Debug::log(__buffer, args);
}

uint64_t wait(const uint64_t WaitFor)
{
    uint64_t curTime = rdcycle64();
    while (curTime < WaitFor)
    {
        curTime = rdcycle64();
        // Busy wait, TODO could change to thread sleep?
    }
    return curTime;
}

void reset_error_seen_and_shown()
{
    errorSeen         = false;
    errorMessageShown = false;
}

void lcd_display_cheri_message()
{
    if (errorSeen && !errorMessageShown)
    {
        errorMessageShown = true;
        Size  displaySize = lcd->resolution();
        Point centre      = {displaySize.width / 2, displaySize.height / 2};

        lcd->draw_str({centre.x - 70, centre.y + 27},
                      "Unexpected CHERI capability violation!",
                      Color::Black,
                      Color::Red);
        lcd->draw_str({centre.x - 65, centre.y + 40},
                      "Memory has been safely protected",
                      Color::Black,
                      Color::Green);
        lcd->draw_str({centre.x - 20, centre.y + 50},
                      "by CHERI.",
                      Color::Black,
                      Color::Green);
        lcd->draw_image_rgb565(
          Rect::from_point_and_size({centre.x + 20, centre.y + 50}, {10, 10}),
          cherryImage10x10);
    }
}

void lcd_draw_str(uint32_t    x,
                  uint32_t    y,
                  LCDFont     font,
                  const char *format,
                  uint32_t    bgColor,
                  uint32_t    fgColor,
                  ...)
{
    // TODO modularise this code, its repeated from the UART stuff, and a huge
    // mess
    va_list args;
    va_start(args, fgColor);
    char __buffer[1024];
    int  strlen = 0;
    // Crudely replace %u manually
    const char *currentChar = format;
    while (*currentChar != '\0')
    {
        if (currentChar[0] == '%' && currentChar[1] == 'u')
        {
            size_t argval = va_arg(args, unsigned int);
            size_t_to_str_base10(&__buffer[strlen], argval);
            while (__buffer[strlen] != '\0')
            {
                strlen++;
            }
            currentChar++;
        }
        else
        {
            __buffer[strlen++] = *currentChar;
        }
        currentChar++;
    }
    __buffer[strlen] = '\0';
    if (__buffer[strlen - 1] == '\n')
    {
        __buffer[--strlen] = '\0';
    }
    va_end(args);

    Font str_font;
    switch (font) {
        case LucidaConsole_10pt:
            str_font = Font::LucidaConsole_10pt;
            break;
        case LucidaConsole_12pt:
            str_font = Font::LucidaConsole_12pt;
            break;
        default:
            str_font = Font::M3x6_16pt;
    }
    lcd->draw_str({x, y},
                  __buffer,
                  static_cast<Color>(bgColor),
                  static_cast<Color>(fgColor),
                  str_font);
}

void lcd_clean(uint32_t color)
{
    lcd->clean(static_cast<Color>(color));
}

void lcd_fill_rect(uint32_t x,
                   uint32_t y,
                   uint32_t w,
                   uint32_t h,
                   uint32_t color)
{
    lcd->fill_rect(Rect::from_point_and_size({x, y}, {w, h}),
                   static_cast<Color>(color));
}

void lcd_draw_img(uint32_t       x,
                  uint32_t       y,
                  uint32_t       w,
                  uint32_t       h,
                  const uint8_t *data)
{
    lcd->draw_image_rgb565(Rect::from_point_and_size({x, y}, {w, h}), data);
}

uint8_t read_joystick()
{
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);
    return static_cast<uint8_t>(gpio->read_joystick());
}

bool read_pedal_digital()
{
    auto gpio = MMIO_CAPABILITY(SonataGPIO, gpio);
    return (gpio->input & (1 << 13)) > 0;
}

uint32_t read_pedal_analogue()
{
    // To allow the user to put the pedal pin in any of the analogue Arduino
    // pins, read all measurements and take the max.
    uint32_t maxPedalValue = 0;

    const SonataAdc::MeasurementRegister Pins[6] = {
      SonataAdc::MeasurementRegister::ArduinoA0,
      SonataAdc::MeasurementRegister::ArduinoA1,
      SonataAdc::MeasurementRegister::ArduinoA2,
      SonataAdc::MeasurementRegister::ArduinoA3,
      SonataAdc::MeasurementRegister::ArduinoA4,
      SonataAdc::MeasurementRegister::ArduinoA5
    };
    for (auto pin : Pins)
    {
        maxPedalValue = MAX(maxPedalValue, adc->read_last_measurement(pin));
    }
    Debug::log("ANALOGUE: {}", (int)maxPedalValue);
    // Linearly transform the analogue range for our pedal to the range needed
    // for the demo.
    uint32_t pedal = 0;
    if (maxPedalValue > PEDAL_MIN_ANALOGUE)
    {
        pedal = maxPedalValue - PEDAL_MIN_ANALOGUE;
    }
    pedal *= (DEMO_ACCELERATION_PEDAL_MAX - DEMO_ACCELERATION_PEDAL_MIN);
    pedal /= (PEDAL_MAX_ANALOGUE - PEDAL_MIN_ANALOGUE);
    pedal += DEMO_ACCELERATION_PEDAL_MIN;

    return pedal;
}

void null_callback(){};

bool null_ethernet_callback(uint8_t *buffer, uint16_t length)
{
    return true;
};

void send_ethernet_frame(const uint8_t *buffer, uint16_t length)
{
    uint8_t *frameBuf =
      static_cast<uint8_t *>(malloc(sizeof(uint8_t) * length));
    for (uint16_t i = 0; i < length; ++i)
    {
        frameBuf[i] = buffer[i];
    }
    if (!ethernet->send_frame(frameBuf, length, null_ethernet_callback))
    {
        Debug::log("Error sending frame...");
    }
    free(frameBuf);
}

static TaskTwo memTaskTwo = {
  .write = {0},
};
static TaskOne memTaskOne = {
  .acceleration = 12,
  .braking      = 2,
  .speed        = 0,
};

static AnalogueTaskTwo memAnalogueTaskTwo = {
    .framebuffer = {0},
};
static AnalogueTaskOne memAnalogueTaskOne = {
    .acceleration = 12,
    .braking = 2,
};

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
    auto [exceptionCode, registerNumber] = extract_cheri_mtval(mtval);
    if (exceptionCode == CauseCode::BoundsViolation ||
        exceptionCode == CauseCode::TagViolation)
    {
        if (!errorSeen)
        {
            // Two violations - one bound, one tag. Only show an error once
            errorSeen = true;
            Debug::log("Unexpected CHERI capability violation!");
            Debug::log("Memory has been safely protected by CHERI.");
        }
        frame->pcc = (void *)((uint32_t *)(frame->pcc) + 1);
        return ErrorRecoveryBehaviour::InstallContext;
    }

    Debug::log(
      "Unexpected CHERI Capability violation encountered. Stopping...");
    return ErrorRecoveryBehaviour::ForceUnwind;
}

// Thread entry point.
void __cheri_compartment("automotive") entry()
{
    lcd               = new SonataLcd();
    Size  displaySize = lcd->resolution();
    Point centre      = {displaySize.width / 2, displaySize.height / 2};

    // Initialise ethernet driver for use via callback
    ethernet = new EthernetDevice();
    ethernet->mac_address_set();
    if (!ethernet->phy_link_status())
    {
        Debug::log("Waiting for a good physical ethernet link...\n");
        lcd->clean(Color::Black);
        lcd->draw_str({centre.x - 55, centre.y - 5},
                      "Waiting for a good physical",
                      Color::Black,
                      Color::White);
        lcd->draw_str({centre.x - 30, centre.y + 5},
                      "ethernet link...",
                      Color::Black,
                      Color::White);
    }
    while (!ethernet->phy_link_status()) {
        thread_millisecond_wait(50);
    }
    thread_millisecond_wait(250);

    // Initialise ADC driver for use via callback
    SonataAdc::ClockDivider adcClockDivider =
      (CPU_TIMER_HZ / SonataAdc::MinClockFrequencyHz);
    adc = new SonataAdc(adcClockDivider, SonataAdc::PowerDownMode::None);

    // Adapt common automotive library for CHERIoT drivers
    const uint32_t CyclesPerMillisecond = CPU_TIMER_HZ / 1000;
    init_lcd(displaySize.width, displaySize.height);
    init_callbacks({
      .uart_send           = write_to_uart,
      .wait                = wait,
      .wait_time           = 120 * CyclesPerMillisecond,
      .time                = rdcycle64,
      .loop                = lcd_display_cheri_message,
      .start               = reset_error_seen_and_shown,
      .joystick_read       = read_joystick,
      .digital_pedal_read  = read_pedal_digital,
      .analogue_pedal_read = read_pedal_analogue,
      .ethernet_transmit   = send_ethernet_frame,
      .lcd =
        {
          .draw_str        = lcd_draw_str,
          .clean           = lcd_clean,
          .fill_rect       = lcd_fill_rect,
          .draw_img_rgb565 = lcd_draw_img,
        },
    });

    DemoApplication option;
    while (true)
    {
        // Run demo selection
        option = select_demo();

        // Run automotive demo
        switch (option)
        {
            case NoPedal:
                // Run simple timed demo with no pedal & using passthrough
                init_no_pedal_demo_mem(&memTaskOne, &memTaskTwo);
                run_no_pedal_demo(rdcycle64());
                break;
            case JoystickPedal:
                // Run demo using a joystick as a pedal, with passthrough
                init_joystick_demo_mem(&memTaskOne, &memTaskTwo);
                run_joystick_demo(rdcycle64());
                break;
            case DigitalPedal:
                // Run demo using an actual physical pedal but taking
                // a digital signal, using simulation instead of passthrough
                init_digital_pedal_demo_mem(&memTaskOne, &memTaskTwo);
                run_digital_pedal_demo(rdcycle64());
                break;
            case AnaloguePedal:
                // Run demo using an actual physical pedal, taking an
                // analogue signal via an ADC, with passthrough.
                init_analogue_pedal_demo_mem(&memAnalogueTaskOne, &memAnalogueTaskTwo);
                run_analogue_pedal_demo(rdcycle64());
                break;
        }
    }

    // Cleanup
    delete lcd;
    delete ethernet;
}
