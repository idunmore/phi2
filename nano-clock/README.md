# nano-clock - Arduino Nano based System Clock Gen

[![Static Badge](https://img.shields.io/badge/Arduino-00878F?logo=arduino&logoColor=fff&label=Arduino%20Nano&link=https://store-usa.arduino.cc/products/arduino-nano)](https://store-usa.arduino.cc/products/arduino-nano/)&nbsp;[![License](https://img.shields.io/badge/license-MIT-green)](./LICENSE)


**nano-clock** is the first project in the "phi2" series.

It was built to provide a more flexible alternative to [Ben Eater's](https://eater.net) [6502 clock-module](https://eater.net/6502), both for single-step and slow-cycle operations as well as the ability to replace the final build's 1MHz crystal oscillator while offering faster clock speeds.

![nano-clock breadboard layout](images/nano-clock_breadboard.png)

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
- 14-segment 4-character LED status/info display (in keeping with the "retro" theme)
  - Brightness control
  - Clock-Running indication
- Save Settings for Auto-Start

## Usage

The interface for **nano-clock** comprises for buttons and a 4-digit, 14-segment, alphanumeric LED display.  Each button has two functions, driven by a short-press (<999ms) or a long-press (>=1s).  From left to right, the button functions are as follows:

### Button 1 - Step/Run Toggle | Step vs. Clock Mode

A long press switches between **Single Step** mode (STEP) and **Clock** (CLCK) mode.

Depending on the mode, a short press does one of two things:

- In Step mode, pressing button one generates a single clock pulse.  This is indicated by the periods on the LED display illuminating.
- In Clock mode, it toggles between Run (starting) and Stop (stopping) behavior for the clock.

### Button 2 - Frequency Select | Frequency Band Select

- A short press steps through the available frequencies in the current frequency band.
- A long press steps through the available frequency bands (Hz, kHz and MHz).

In both cases, if you reach the end of the available frequencies or bands, the next press will take you back to the first.

### Button 3 - Reset | Auto Reset Toggle

- A short press generates a RESET event (brings the RESET PIN to LOW for the duration specified; default is 2ms per the WDC65C02).
- A long press toggles between RsON and RsOFF, which determine whether a RESET cycle is executed when the nano-clock first powers on.  This is used to allow nano-clock to control intial power on and clock management.  Note that this value only has an effect if it is SAVED.

### Button 4 - Brightness | Save Settings

- A short press steps through the 16 brightness levels (0 to 15).  There is no "OFF" setting.  Depending on the specific LED module you are using, settings above 8 do not appear meaningfully different.  If you press button 4 when the brightness is 15, it will return to zero.
- A long press SAVEs the current state of **nano-clock** to EEPROM.  This will be reloaded on a reset or power off/on cycle so that **nano-clock** can be used to both reset and initiate operation of a machine without further interaction.

*Note:* If you want **nano-clock** to automatically start from cold/reset with the clock RUNNING, you must perform the SAVE while the clock is in Run mode and RUN is toggled on (frequency display is blinking).

## Dependencies

A formal dependency list, with minimum versions, will be added here prior to the first stable release, however the basic library dependencies are:

- EasyButton
- Adafruit_GFX
- Adafruit_LEDBackpack

## Status

All functionality implemented.
