# Instructions
**These are instructions for how to run the automotive demonstration in its 
*current* state. This may (and is likely) to change**.

## Requirements
To run the demo, you need the following:

1. **2 Sonata boards** that support **Ethernet** (Revision 9 or greater). One
board will be your **sending** board with the **bug** (**Board 1**), and one
board will just be a **receiving** board without any bug (**Board 2**).
2. An **ethernet cable**, which you should run between your two Sonata boards.
3. 2 USB cables to power your Sonata boards.
4. A **model car**, with a GROUND wire and an input variable wire that can take
PWM. Ground should be connected to a ground on **Board 2**, and the other wire
should be connected to the **PWM pin** on **Board 2** also. This is located at
the top of P7, in the mikro BUS section in the centre of the board.
5. Something to prop up the car, so that it doesn't actually drive during the
demo and go flying off (or, just stand the car upside down).

## Setup

**To load the receiving firmware onto Board 2**:
 - Plug **only** Board 2 in via USB.
 - Make sure you have an updated bitstream also, otherwise this won't work.
 - Apply the following patch to the `sonata-prerelease.json` file in the 
 `cheriot-rtos` module sub-directory located below the repository root. This
 patch simply exposes the PWM as MMIO so that it can be used by the receiving
 software.
    ```diff
    diff --git a/sdk/boards/sonata-prerelease.json b/sdk/boards/sonata-prerelease.json
    index 1df5862..38582f8 100644
    --- a/sdk/boards/sonata-prerelease.json
    +++ b/sdk/boards/sonata-prerelease.json
    @@ -63,6 +63,10 @@
            "plic": {
                "start" : 0x88000000,
                "end"   : 0x88400000
    +        },
    +        "pwm": {
    +            "start" : 0x80001000,
    +            "length": 0x00001000
            }
        },
        "instruction_memory": {
    ```
 - Navigate to the root of the `sonata-software` repository.
 - Make sure you are in a `nix develop` environment.
 - Run `xmake -P examples` to build all example firmware.
 - Copy over the `automative_demo_receive.uf2` build output from the 
 `build/cheriot/cheriot/release` directory into the SONATA drive. This command
 might look something like (still at the project root):
 
    ```bash
    cp build/cheriot/cheriot/release/automative_demo_receive.uf2 /media/yourname/SONATA
    ```
 - If you did everything correctly, the LCD should light up and start to
 display the car's current speed (0).

**To run the sending firmware in CHERIoT mode:**
 - Now, **unplug** Board 2 (the software will persist), and plug in **only** 
 Board 1 via USB.
 - Again, make sure you have an updated bitstream, otherwise this won't work.
 - Same as before, in the root of the `sonata-software` repository, be in the
 `nix develop` environment and run `xmake -P examples`. If you already loaded
 the receiving firmware, then **you have already done this**.
 - Now copy over the `automative_demo.uf2` build output. This command might

 look something like (still at project root):
    ```bash
    cp build/cheriot/cheriot/release/automative_demo.uf2 /media/yourname/SONATA
    ```
 - If you did everything correctly, the LCD Should light up and display a 
 message like "*Waiting for a good ethernet link...*".
 - Now, if you plug in **Board 2** via USB (whilst still keeping Board 1 
 connected), and with the Ethernet cable plugged into both boards, this
 should change to a menu titled something like "*Select Demo Application*".
   - If this doesn't happen, then the Ethernet link can't be seen for some 
   reason. Make sure you power both boards correctly, and that there is
   an Ethernet cable connecting both boards, and that both boards have the
   most up-to-date bitstream.
 - If you try running Demo 1 (Simple, No pedal), you should observe the car
 initially moving slowly at a speed of `15`, but suddenly at a count of 100
 a bug in the code attempts to overwrites the pedal speed to `1000`. 
 However, as is shown on the LCD display (and the CHERI capability violation 
 LEDs), this exception was caught and the overwrite did not take place
 thanks to CHERI, so the car continues at the same speed. You can press the 
 joystick on **Board 2** after the demo ends to reset the car's speed.

**To run the sending firmware in legacy (non-CHERIoT mode):**
 - Plug in **only Board 1** via USB.
 - You also need to obtain an updated version of the bitstream with CHERI
 disabled.
   - Go to the `sonata-system` repository, and apply the following patches:

        ```diff
        diff --git a/dv/verilator/top_verilator.sv b/dv/verilator/top_verilator.sv
        index fe16535..406a467 100644
        --- a/dv/verilator/top_verilator.sv
        +++ b/dv/verilator/top_verilator.sv
        @@ -7,7 +7,7 @@ module top_verilator (input logic clk_i, rst_ni);
        
        localparam ClockFrequency = 30_000_000;
        localparam BaudRate       = 921_600;
        -  localparam EnableCHERI    = 1'b1;
        +  localparam EnableCHERI    = 1'b0;  // Patchwork: temporarily disable CHERI.
        
        logic uart_sys_rx, uart_sys_tx;
        
        diff --git a/rtl/fpga/top_sonata.sv b/rtl/fpga/top_sonata.sv
        index 032dc3f..3a0721d 100644
        --- a/rtl/fpga/top_sonata.sv
        +++ b/rtl/fpga/top_sonata.sv
        @@ -291,7 +291,7 @@ module top_sonata (
        
        // Enable CHERI by default.
        logic enable_cheri;
        -  assign enable_cheri = 1'b1;
        +  assign enable_cheri = 1'b0;  // Patchwork: temporarily disable CHERI.
        
        logic rgbled_dout;
        
        ```
   - Make sure you convert the bitstream to a UF2 using the updated RP2040
   firmware ID, i.e. using

     ```bash
     uf2conv build/lowrisc_sonata_system_0/synth-vivado/lowrisc_sonata_system_0.bit -f 0x4240BDE -c
     ```
   - You can tell that CHERI is disabled because the red `LEGACY` LED on
   the board should be lit, rather than the green `CHERI` LED.
 - Navigate to the root of the `sonata-system` repository. Note that right
 now this is the `system` repo, and *not* the `software` repository.
 - Make sure you are in the `nix develop` environment.
 - Generate the legacy software cmake BUILD files using 
 
    ```bash
    cmake -B sw/legacy/build/ -S sw/legacy/
    ```
 - Build the legacy software with cmake using

    ```bash
    cmake --build sw/legacy/build/
    ```
  - Load the build baremetal sending software onto Board 1 with the following command:

    ```bash
    uf2conv build/lowrisc_sonata_system_0/synth-vivado/lowrisc_sonata_system_0.bit -f 0x4240BDE -c
    ```
    - Note that this software is **non-persistent**. Every time you reset / power cycle the board,
    you will have to run this command *again* to load the legacy demo software. This is unlike the
    CHERI software, which is **persistent** and does not need to be reloaded so long as you do
    not accidentally overwrite it.
 - If you did everything correctly, the LCD Should light up and display a 
 message like "*Waiting for a good ethernet link...*".
 - Now, if you plug in **Board 2** via USB (whilst still keeping Board 1 
 connected), and with the Ethernet cable plugged into both boards, this
 should change to a menu titled something like "*Select Demo Application*".
   - If this doesn't happen, then the Ethernet link can't be seen for some 
   reason. Make sure you power both boards correctly, and that there is
   an Ethernet cable connecting both boards, and that both boards have
   up-to-date bitstreams.
 - If you try running Demo 1 (Simple, No pedal), you should observe the car
 initially moving slowly at a speed of `15`, but suddenly at a count of 100
 a bug in the code overwrites the pedal speed to `1000`, making the car 
 move at max speed (*oh no!*). You can press the joystick on **Board 2** 
 after the demo ends to reset the car's speed.
