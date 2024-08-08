-- Copyright lowRISC Contributors.
-- SPDX-License-Identifier: Apache-2.0

set_project("Sonata Examples")
sdkdir = "../cheriot-rtos/sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

includes(path.join(sdkdir, "lib"))
includes("../libraries")
includes("../common.lua")

option("board")
    set_default("sonata-prerelease")

includes("all", "snake", "automotive")

-- A simple demo using only devices on the Sonata board
firmware("sonata_simple_demo")
    add_deps("freestanding", "led_walk_raw", "echo", "lcd_test")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "led_walk_raw",
                priority = 2,
                entry_point = "start_walking",
                stack_size = 0x200,
                trusted_stack_frames = 1
            },
            {
                compartment = "echo",
                priority = 1,
                entry_point = "entry_point",
                stack_size = 0x200,
                trusted_stack_frames = 1
            },
            {
                compartment = "lcd_test",
                priority = 2,
                entry_point = "lcd_test",
                stack_size = 0x1000,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)
    after_link(convert_to_uf2)

-- A demo that expects additional devices such as I2C devices
firmware("sonata_demo_everything")
    add_deps("freestanding", "led_walk_raw", "echo", "lcd_test", "i2c_example")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "led_walk_raw",
                priority = 2,
                entry_point = "start_walking",
                stack_size = 0x200,
                trusted_stack_frames = 1
            },
            {
                compartment = "echo",
                priority = 1,
                entry_point = "entry_point",
                stack_size = 0x200,
                trusted_stack_frames = 1
            },
            {
                compartment = "lcd_test",
                priority = 2,
                entry_point = "lcd_test",
                stack_size = 0x1000,
                trusted_stack_frames = 1
            },
            {
                compartment = "i2c_example",
                priority = 2,
                entry_point = "run",
                stack_size = 0x300,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)
    after_link(convert_to_uf2)

-- A firmware image that only walks LEDs
firmware("sonata_led_demo")
    add_deps("freestanding", "debug")
    add_deps("led_walk", "gpiolib")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "led_walk",
                priority = 1,
                entry_point = "start_walking",
                stack_size = 0x400,
                trusted_stack_frames = 3
            }
        }, {expand = false})
    end)
    after_link(convert_to_uf2)

firmware("proximity_test")
    add_deps("freestanding", "proximity_sensor_example")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "proximity_sensor_example",
                priority = 2,
                entry_point = "run",
                stack_size = 0x200,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)
    after_link(convert_to_uf2)

-- Snake demo
firmware("snake_demo")
    add_deps("freestanding", "snake")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "snake",
                priority = 2,
                entry_point = "snake",
                stack_size = 0x1000,
                trusted_stack_frames = 2
            }
        }, {expand = false})
    end)
    after_link(convert_to_uf2)

-- Automotive demo
firmware("automotive_demo")
    add_deps("freestanding", "automotive", "bad") --"modified_snake_temp")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
        {
            compartment = "automotive",
            priority = 1,
            entry_point = "automotive_entry",
            stack_size = 0x1000,
            trusted_stack_frames = 3
        }
    }, {expand = false})
    end)
    after_link(convert_to_uf2)
