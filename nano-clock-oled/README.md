# nano-clock-oled - Arduino Nano based System Clock Gen

[![Static Badge](https://img.shields.io/badge/Arduino-00878F?logo=arduino&logoColor=fff&label=Arduino%20Nano&link=https://store-usa.arduino.cc/products/arduino-nano)](https://store-usa.arduino.cc/products/arduino-nano/)&nbsp;[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)


**nano-clock-oled** is the fourth project in the "phi2" series.

It was built to provide a more flexible alternative to [Ben Eater's](https://eater.net) [6502 clock-module](https://eater.net/6502), both for single-step and slow-cycle operations as well as the ability to replace the final build's 1MHz crystal oscillator while offering faster clock speeds.

![nano-clock-oled breadboard layout](images/nano-clock-oled_breadboard.png)

## Features

- Single-step and Clock modes
- Run/Stop toggle
- Selectable frequencies across three bands (Hz, kHz and MHz)
  - Hz Band - 1, 2, 5, 10, 25, 50, 100, 250 & 500Hz
  - kHz Band - 1, 2, 5, 10, 25, 50, 100, 250 & 500kHz
  - MHz Band - 1, 2, 4, 8MHz
  - Easy to add additional bands/frequencies
- Auto and manual CPU reset functionality
  - w/ Auto-reset on Start option
- 128x64 pixel OLED status/info display
  - Display ON/OFF control
  - Clock-Running and Status indication
- Save Settings for Auto-Start

## Status

This is a currently **work-in-progress**.

**nano-clock-oled** uses the core "clock" and "features" implementation from my original [nano-clock](https://github.com/idunmore/phi2/tree/main/nano-clock) project.  These are _here and functional_, though not thoroughly tested yet.

Preliminary setup and utilization of the OLED display, including clock-running "waveform" animation, and various function/status messages have been convertd to use the OLED display instead of the 14-segment LED.

Now, the display routines need to be refactored and adjusted into a single state-aware display "controller", to work with the minimal-memory buffer mode of the u8g2 display/rendering library (a single 1-bit buffer for the 128x64 pixel display would require 1KB, which is half the Nano's available RAM, so I am using the paged-mode in u8g2.  The top, yellow, status-line/flag display also needs to be implemented.

## Usage

### Connecting to your Computer

Ensure you tie the GND (black) busses to the common GND for your computer board.  If you also connect the +5V busses to your computer's supply, it can power both itself and the **nano-clock-oled**.  Then connect the CLOCK pin (9) on the **nano-clock** to the clock pin on your computer's CPU/MPU.  In the case of the Ben Eater WDC65C02 based computer, this would be **phi2** (pin 37).

If you want RESET functionality as well, connect the RESET pin (8) on the **nano-clock-oled** to the RESET pin on your computer's CPU/MPU.  RESET is a LOW trigger, so the line will be high when not resetting (no pull-up resistor should be required).

### User Interface/Controls

The interface for **nano-clock-oled** comprises four buttons and a 128x64 pixed graphics-capable OLED display.  Each button has two functions, driven by a short-press (<999ms) or a long-press (>=1s).  From left to right, the button functions are as follows:

#### Button 1 - Step/Run Toggle | Step vs. Clock Mode

A long press switches between **Single Step** mode (STEP) and **Clock** (CLCK) mode.

Depending on the mode, a short press does one of two things:

- In Step mode, pressing button one generates a single clock pulse.  This is indicated by the periods on the LED display illuminating.
- In Clock mode, it toggles between Run (starting) and Stop (stopping) behavior for the clock.

#### Button 2 - Frequency Select | Frequency Band Select

- A short press steps through the available frequencies in the current frequency band.
- A long press steps through the available frequency bands (Hz, kHz and MHz).

In both cases, if you reach the end of the available frequencies or bands, the next press will take you back to the first.

#### Button 3 - Reset | Auto Reset Toggle

- A short press generates a RESET event (brings the RESET PIN to LOW for the duration specified; default is 2ms per the WDC65C02).
- A long press toggles between RsON and RsOFF, which determine whether a RESET cycle is executed when the nano-clock first powers on.  This is used to allow nano-clock-oled to control initial power on and clock management.  Note that this value only has an effect if it is SAVED.

#### Button 4 - Brightness | Save Settings

- A short press steps through the 16 brightness levels (0 to 15).  There is no "OFF" setting.  Depending on the specific LED module you are using, settings above 8 do not appear meaningfully different.  If you press button 4 when the brightness is 15, it will return to zero.
- A long press SAVEs the current state of **nano-clock-oled** to EEPROM.  This will be reloaded on a reset or power off/on cycle so that **nano-clock-oled** can be used to both reset and initiate operation of a machine without further interaction.

*Note:* If you want **nano-clock-oled** to automatically start from cold/reset with the clock RUNNING, you must perform the SAVE while the clock is in Run mode and RUN is toggled on (frequency display is blinking).

## Dependencies

A formal dependency list, with minimum versions, will be added here prior to the first stable release, however the basic library dependencies are:

- EasyButton (v2.0.3)
- 8u2g (v2.36.19)

## Building a NANO-CLOCK-OLED

### Components

A minimal number of components are required to assemble **nano-clock**:

- Arduino Nano v3.0 (clones are fine; must match official pinout)
- 4x 6x6mm Momentary Tactile Switches
- 0.96" OLED Display w/ I2C Interface
  - 5v -> 3.3v Level Shifters (only required for 3.3v OLED displays)
- 830 Point Solderless Breadboard

I used an [Elegoo Arduino Nano v3.0](https://www.amazon.com/ELEGOO-Presoldered-Compatible-Arduino-Microcontroller/dp/B0F6Y7GS4Q/), since they are cheap and have a USB-C connector (they have options for other connectors, so verify which version you order).


### Assembly

The breadboard "schematic", above, shows all of the required connections.  In an actual build, you can run the non-ground connections to the Arduino Nano *under* the board (since it is on headers) for a cleaner and more secure fit.  Similarly, the SDA and SCL connections to the LED display can be run under the backpack PCB.

![nano-clock-oled breadboard layout](images/nano-clock-oled_actual.jpg)

### Programming the Arduino Nano

Install the libraries described in "Dependencies" via the Arduino IDE, connect your board to your computer and choose the appropriate connection and board type ("Arduino Nano"), load the `nano-clock-oled.ino` file, and click "Upload".  The sketch will be verified, compiled, and uploaded to your Arduino Nano.

If everything has worked, after a brief pause, the OLED display should show "1Hz".

That's it.
