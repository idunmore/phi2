# phi2 - Retro/Homebrew Computer Focused Clock Generators

[![Static Badge](https://img.shields.io/badge/Arduino-00878F?logo=arduino&logoColor=fff&label=Arduino%20Nano&link=https://store-usa.arduino.cc/products/arduino-nano)](https://store-usa.arduino.cc/products/arduino-nano/)&nbsp;![GitHub License](https://img.shields.io/github/license/idunmore/phi2)

**phi2** is a collection of breadboard-buildable, high-flexible, clock generators for retro/homebrew computers, using a minimal number of inexpensive and readily available micro-controllers, SBCs and other parts.

### Why "phi2"?

This is a reference to the clock input on my favorite early CPU/MPU, the [6502](https://en.wikipedia.org/wiki/MOS_Technology_6502). While 6502-based builds are *my* current focus, the clock generators here can be used with effectively any CPU/computer build.

## Structure of this Repository

A simple mono-repo, with sub-projects on a per-build basis.

Due to the typically resource-constrained nature of the smaller microcontrollers, and significant differences between architectures and, even within a common architecture, different capabilities and pin-outs, shared code is likely to be limited.  Thus each project may be considered a self-contained whole, unless otherwise noted.  This helps minimize code size and complexity and improves code readability by avoiding the otherwise-necessary multiple levels of conditional compilation that would otherwise be required.

Each individual project will include both hardware build details and diagrams along with ready-to-run firmware/software.

## Clock Features

These will vary per-project.

### nano-clock

This is the first project in the series.

It has single-step and clock modes, run-stop toggle, selectable frequencies across three bands (Hz, kHz and MHz) in various increments from 1Hz to 8MHz, auto and manual reset functionality (with auto-reset on start option, configurable at run-time), a 14-segment 4-character LED status/info display (in keeping with the "retro" theme) with brightness control and run-time indicators.  This is a simple build, using just an Arduino Nano (an inexpensive clone will work fine) and four tactile buttons.

Advanced variations of this will incorporate different display technologies including tiny OLED and TFT color displays, and rich menu/graphical interfaces.

### nano-clock-core

The simplest (just the core capabilities) project provides run-time selectable (and saveable) 1MHz, 2MHz, 4MHz and 8MHz clock frequencies, auto-starts when powered on, and offers a compile-time flag to enable/disable auto-reset behavior, and indicates operations using the built-in Arduino LED.  It requires only an Arduino Nano (or clone) and a single tactile button, which can be removed once an operating frequency has been selected and saved.

**nano-clock-core** is not much bigger than a standard 4-pin crystal oscillator can.

## AI Usage

**None** for these projects, unless noted otherwise.

The goal wasn't to build them quickly, it was to have fun doing so and learn as much about the low-level hardware as possible.  That often means hand-optimized coding based on information buried deep in the datasheets for the raw microcontroller cores.

In most cases it also required a fair degree of manual tuning, changing values on the fly while observing output performance across several physical devices on a high-speed oscilloscope.  Variations within quoted tolerance are readily visible here.
