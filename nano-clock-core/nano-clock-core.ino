// Nano-Clock-Core - Minimalist Arduino Nano based system clock generator
//
// Copyright (C) 2026, Ian Dunmore
//
// License: MIT (https://github.com/idunmore/phi2/blob/main/LICENSE)

// Core Components
#include <Wire.h>
#include <EEPROM.h>

// External Libraries
#include <EasyButton.h>

// Interface Definitions

// Built-in LED
#define BUILT_IN_LED_PIN 13

// Button Pins
#define FREQ_BUTTON 2
#define STEP_BUTTON 3

// Long Press Duration (in milliseconds)
#define LONG_PRESS_DURATION 1000

// Output Pins
#define RESET_PIN 8
#define CLOCK_PIN 9

// State Transition Durations
#define RESET_DURATION         2   // Time to hold RESB low (in milliseconds)
#define STEP_PULSE_LENGTH      2   // Time to hold Phi2 low (in milliseconds)
#define STEP_BLINK_DURATION    20  // Time to light LED on STEP (in milliseconds)
#define PRE_RESET_SETTLE_TIME  500 // Time to allow Nano to "settle" (in milliseconds)

// LED BLINK Values - the product of these two values MUST be EVEN!
#define SAVE_BLINK_COUNT    30  // Number of times to blink LED on SAVE
#define SAVE_BLINK_DURATION 30  // Time, in milliseconds, between on/off pulses

// Timer Values & Structures

// Timer 1 - Prescaler (Clock Divider) and Timer Mode Values

// Bits 0-2 set the Pre-Scaler Value (RAW Clock Divider)
// Bit 3 sets CTC (Clear Timer on Compare match mode)
#define CTC_PRESCALER_OFF 0b00001001
#define CTC_PRESCALER_8   0b00001010
#define CTC_PRESCALER_64  0b00001011
#define CTS_PRESCALER_256 0b00001100

// TCCR1A - Control Values
#define OC1A_TOGGLE 0b01000000  // Toggle OC1A (Pin 9) on CTC Match
#define OC1A_OFF    0b00000000  // Disable OC1A/OC1B Timer output (Pin 9)

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
  uint8_t tccr1b_flags;       // ATMega328 TCCR1B Timer Control Register - Mode & Prescale Values
  uint16_t timer_count;       // ATMega328 CTC Mode Timer Match value (OCR1A)
  uint16_t blink_rate;         // Frequency indicator is the LED blink rate in milliseconds
                              // (approximately 1000 / (Frequency/1,000,000) )
  Frequency* next_frequency;  // Pointer to next Frequency entry
};

Frequency frequencies[] = {
  {0, CTC_PRESCALER_OFF, 0x0007, 500, &frequencies[1]},  // 1MHz
  {1, CTC_PRESCALER_OFF, 0x0003, 250, &frequencies[2]},  // 2MHz
  {2, CTC_PRESCALER_OFF, 0x0001, 125, &frequencies[3]},  // 4MHz
  {3, CTC_PRESCALER_OFF, 0x0000, 62,  &frequencies[0]},  // 8MHz
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
  TCCR1A = OC1A_OFF;

  // Set the Prescaler and Counter Mode
  TCCR1B = frequency.tccr1b_flags;
  OCR1A  = frequency.timer_count;

  // Enable the Timer's OUTPUT (Toggle Mode)
  if( clock_state.state == States::RUN) {
    TCCR1A = OC1A_TOGGLE;        
  }
}

// Perform the RESET line toggle
void doReset() {
  // Wait for MPU to "settle"
  delay(PRE_RESET_SETTLE_TIME);
  // Perform the RESET
  digitalWrite(RESET_PIN, LOW);
  // This is deliberately blocking!
  delay(RESET_DURATION);
  digitalWrite(RESET_PIN, HIGH);
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
      digitalWrite(BUILT_IN_LED_PIN, saveLEDState);
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
    digitalWrite(BUILT_IN_LED_PIN, LOW);
  }

  // Cycle the clock ONCE, blinking the LED as we go
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(BUILT_IN_LED_PIN, HIGH);
  delay(STEP_PULSE_LENGTH);
  digitalWrite(CLOCK_PIN, HIGH);
  delay(STEP_BLINK_DURATION);
  digitalWrite(BUILT_IN_LED_PIN, LOW);
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
  // LED setup and default to OFF
  pinMode(BUILT_IN_LED_PIN, OUTPUT);
  digitalWrite(BUILT_IN_LED_PIN, ledState);

  // Setup the CLOCK and RESET pins
  pinMode(CLOCK_PIN, OUTPUT);
  digitalWrite(CLOCK_PIN, HIGH);
  pinMode(RESET_PIN, OUTPUT); 

  // We ALWAYS run the RESET cycle; don't connect the RESET_PIN to
  // your computer build if you don't want to use it
  doReset();
  
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

  // Do nothing else if the clock isn't running ...
  if (clock_state.state == States::STOP) return;

  // Clock IS running, so blink the LED at the designated frequency's blink_rate
  if (millis() - lastMillis >= clock_state.currentFrequency().blink_rate) {
    lastMillis = millis();
    ledState = !ledState;   
    digitalWrite(BUILT_IN_LED_PIN, ledState);  
  }  
}
