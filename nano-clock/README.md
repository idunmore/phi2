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

## Dependencies

A formal dependency list, with minimum versions, will be added here prior to the first stable release, however the basic library dependencies are:

- EasyButton
- Adafruit_GFX
- Adafruit_LEDBackpack

## Status

All functionality implemented.

An optimization pass is required since as-is various LED display and other state changes are duplicated in various events and the load routine.  These behaviors need to be moved to a common "update state" function, leaving the event handlers and load routine to focus purely on mutating state.  In the current state some functions put the display "back" in frequency display mode instead of STEP mode, which will also be fixed as part of this optimization.
