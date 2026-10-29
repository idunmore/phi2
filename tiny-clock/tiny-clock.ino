// Tiny-Clock-Clock - Minimalist ATtiny85/Arduino  based system clock generator
//
// Copyright (C) 2026, Ian Dunmore
//
// License: MIT (https://github.com/idunmore/phi2/blob/main/LICENSE)

// ISP Programmer/Fuse/Bootloader Settings

// Board:       ATTiny25/45/85 (https://github.com/MCUdude/TinyCore)
//              (since the commonly usesd SpenceKonde ATTinyCore hasn't
//              been updated in years)
// Bootloader:  Urboot
// Chip:        ATTiny85
// Clock:       8MHz internal osc.
// EEPROM:      EEPROM retained

// Core Components
#include <Wire.h>
#include <EEPROM.h>

// External Libraries
#include <EasyButton.h>

// Interface Definitions

// LED Indicator
#define LED_PIN 2

// Button Pins
#define FREQ_BUTTON 3
#define STEP_BUTTON 4

// Long Press Duration (in milliseconds)
#define LONG_PRESS_DURATION 1000

// Output Pins
#define CLOCK_PIN 0

// State Transition Durations
#define STEP_PULSE_LENGTH      2   // Time to hold Phi2 low (in milliseconds)
#define STEP_BLINK_DURATION    20  // Time to light LED on STEP (in milliseconds)
#define PRE_RESET_SETTLE_TIME  500 // Time to allow MCU to "settle" (in milliseconds)

// LED BLINK Values - the product of these two values MUST be EVEN!
#define SAVE_BLINK_COUNT    30  // Number of times to blink LED on SAVE
#define SAVE_BLINK_DURATION 30  // Time, in milliseconds, between on/off pulses

// Timer Values & Structures

// PLL Control/Status

// Bit  7   sets LSM (0)
// Bits 3-6 are reserved
// Bit  0   sets PLOCK the PLL Lock Detector, set when locked to REF clock
// Bit  1   sets PLLE Enable/Disable (ENABLE)
// Bit  2   sets PCKE Enable/Disable (ENABLE)
#define PLLCSR_OPTIONS 0b00000110;

// Timer 1 - Prescaler (Clock Divider) and Timer Mode Values

// Bits 0-3 set the Pre-Scaler Value (CK Divider), and vary by frequency
#define PRESCALER_STOP 0b00000000
#define PRESCALER_1    0b00000001
#define PRESCALER_2    0b00000010
#define PRESCALER_4    0b00000011

// Bits 4-7 are the same for all frequencies
// Bits 4-5 set the OC1A Pin Behavior (TOGGLE)
// Bit  6   sets PWM Mode (ON)
// Bit  7   sets CTC Enable/Disable (ENABLE)
#define TCCR1_OPTIONS 0b11010000
#define STOP_TIMER1    0x00

// Structures

// Frequency entries are linked lists stored in an array.  The linked
// list allows easy iteration (it is always item = item->next), while the array
// "container" allows direct, indexed, access for save/load functionality.

// Adding a new Frequency requires adding a new item in the array, with
// appropriate values, and adjusting the index and next_* accordingly.

// Frequency - Represents ONE Clock Frequency, as a node in a linked list.  The
// last entry in the list MUST point back to the first!
struct Frequency {  
  uint8_t index;              // Position in array, for Save/Load lookup  
  uint8_t tccr1_flags;        // ATtiny85 TCCR1 Timer Control Register - Mode & Prescale Values
  uint8_t orc1a;              // CTC Time Match Value and PWM High-State Trigger (on OC1A)
                              // Should be as close to HALF of ORC1A as possible for 50% PWM duty cycle
  uint8_t orc1c;              // CTC RESET Match Value and PWM Low-State Trigger (on OC1A)  
                              // Controls overall frequency in conjunction with PRESCALER; must always
                              // be higher than ORC1A
  uint16_t blink_rate;        // Frequency indicator is the LED blink rate in milliseconds
                              // (approximately 1000 / (Frequency/1,000,000) )
  Frequency* next_frequency;  // Pointer to next Frequency entry
};

Frequency frequencies[] = {
  {0, PRESCALER_2 | TCCR1_OPTIONS, 0x10, 0x20, 500, &frequencies[1]},  // 1MHz
  {1, PRESCALER_1 | TCCR1_OPTIONS, 0x10, 0x20, 250, &frequencies[2]},  // 2MHz
  {2, PRESCALER_1 | TCCR1_OPTIONS, 0x09, 0x10, 125, &frequencies[3]},  // 4MHz
  {3, PRESCALER_1 | TCCR1_OPTIONS, 0x04, 0x07,  62, &frequencies[0]},  // 8MHz
};

// Index into frequencies[] for the clock to start in when no saved data exists
// by default this is 0 for 1MHz (see array entries above)
#define INITIAL_FREQUENCY_INDEX 0

// Clock Run State
enum States { RUN, STOP };

// EEPROM storage

// Unlikely value; indicates presence of saved data; MUST differ from other
// nano-clock-* variants
#define EEPROM_SENTINEL 0x15 

// Clock State - The current state of the clock, which is also the record that
// is saved to, and loaded from, EEPROM.  Field order defines the EEPROM layout.
struct ClockState {
  uint8_t sentinel;   // EEPROM_SENTINEL when saved; indicates a valid save
  States state;
  uint8_t frequency;  // Index into the Frequency array
  
  Frequency currentFrequency() { return frequencies[frequency]; }

  void Load();
  void Save();
};

// Default Clock State for first run (will be overwritten if any values were saved)
ClockState clock_state = {0, States::RUN, INITIAL_FREQUENCY_INDEX};

// Core Methods
// Sets the new clock frequency and updates the display
void setFrequency(Frequency frequency) {
  // Disable the Timer's OUTPUT
  TCCR1 = STOP_TIMER1;

  // Setup the Timer for the specified frequency
  OCR1A = frequency.orc1a;
  OCR1C = frequency.orc1c;
  TCNT1 = 0;    

  // Enable the Timer's OUTPUT (Toggle Mode)
  if( clock_state.state == States::RUN) {
    TCCR1 = frequency.tccr1_flags;        
  }

  // Let the settings take effect; re-calling too quickly
  // seems to confuse the clock config.
  delay(50);
}

// Event Handlers

// Next Frequency in current band
void onPressedFreq() { 
  // If we are in CLOCK mode, we want the NEXT frequency ...
  if (clock_state.state == States::RUN) {
   clock_state.frequency = clock_state.currentFrequency().next_frequency->index;
  }
  /// ... otherwise just restart the clock at the current frequency.
  clock_state.state = States::RUN;  
  setFrequency(clock_state.currentFrequency());  
}

// Long press on Freq saves our state to EEPROM
void onLongPressSave() {
    // Write the current state to EEPROM
    clock_state.Save();
    // Rapidly flash the LED to show saving is finished
    bool saveLEDState = true;
    for(int i = 0; i < SAVE_BLINK_COUNT; i++ ) {
      digitalWrite(LED_PIN, saveLEDState);
      delay(SAVE_BLINK_DURATION);
      saveLEDState = !saveLEDState;
    } 
}

// Switch to STEP mode if running, issues STEPs if not.
void onPressedStep() {  
  // Switch to STEP mode if the clock is running
  if (clock_state.state == States::RUN) {
    // Stop the clock.
    clock_state.state = States::STOP;
    setFrequency(clock_state.currentFrequency());
    digitalWrite(CLOCK_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
  }

  // Cycle the clock ONCE, blinking the LED as we go
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);
  delay(STEP_PULSE_LENGTH);
  digitalWrite(CLOCK_PIN, HIGH);
  delay(STEP_BLINK_DURATION);
  digitalWrite(LED_PIN, LOW);
}

// Restart the clock
void onLongPressRun() {
  clock_state.state = States::RUN;
  setFrequency(clock_state.currentFrequency());
}

// Load STARTUP state from EEPROM
void ClockState::Load() {
  // Read EEPROM data
  ClockState data;
  EEPROM.get(0, data);
  // Valid save?
  if (data.sentinel != EEPROM_SENTINEL) return;

  // Valid save, so restore state!
  *this = data;
}

// Save state to EEPROM
void ClockState::Save() {
  sentinel = EEPROM_SENTINEL;
  // Store the data at the start of the EEPROM's memory
  EEPROM.put(0, *this);  
}

// Initialize Global Non-Clock State
uint32_t lastMillis = millis();
bool ledState = false;

// Button PIN definitions
EasyButton freqButton(FREQ_BUTTON);
EasyButton stepButton(STEP_BUTTON);

// Initialize pins, display and startup state
void setup() {
  // Initialize the PLL/CLOCK options
  PLLCSR = PLLCSR_OPTIONS;

  // LED setup and default to OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);

  // Setup the CLOCK pin
  pinMode(CLOCK_PIN, OUTPUT);  
  digitalWrite(CLOCK_PIN, HIGH);  
  
  // Setup the FREQ/SAVE button
  freqButton.begin();
  freqButton.onPressed(onPressedFreq);
  freqButton.onPressedFor(LONG_PRESS_DURATION, onLongPressSave);

  // Setup the STEP/RUN button
  stepButton.begin();
  stepButton.onPressed(onPressedStep);
  stepButton.onPressedFor(LONG_PRESS_DURATION, onLongPressRun);

  // Load and Apply saved state
  clock_state.Load();

  // Start the clock etc. based on loaded state
  setFrequency(clock_state.currentFrequency());  
}

// Main program loop
void loop() {
  // Check button states
  freqButton.read();
  stepButton.read();

  // Clock IS running, so blink the LED at the designated frequency's blink_rate
  if (millis() - lastMillis >= clock_state.currentFrequency().blink_rate) {
    lastMillis = millis();
    ledState = !ledState;   
    digitalWrite(LED_PIN, ledState);  
  }  
}
