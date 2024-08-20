-- Copyright lowRISC Contributors.
-- SPDX-License-Identifier: Apache-2.0

compartment("automotive")
    add_deps("lcd", "debug")
    add_files("main.cc", "lib/automotive_common.c", "lib/automotive_menu.c", "lib/no_pedal.c", "lib/joystick_pedal.c", "lib/digital_pedal.c", "lib/analogue_pedal.c")

compartment("automotive_receive")
    add_deps("lcd", "debug")
    add_files("receive.cc", "lib/automotive_common.c")
