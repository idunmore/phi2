// Nano-Clock - Arduino Nano based system clock generator
//
// Copyright (C) 2026, Ian Dunmore
//
// License: MIT (https://github.com/idunmore/phi2/blob/main/LICENSE)

// Core Components
#include <Wire.h>
#include <EEPROM.h>

// External Libraries
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <EasyButton.h>

// Interface Definitions

// Button Pins
#define STEP_BUTTON  4
#define FREQ_BUTTON  5
#define RESET_BUTTON 6
#define SAVE_BUTTON  7

// Long Press Duration (in milliseconds)
#define LONG_PRESS_DURATION 1000

// Output Pins
#define RESET_PIN 8
#define CLOCK_PIN 9

// State Transition Durations
#define RESET_DURATION    2  // Time to hold RESB low (in milliseconds)
#define STEP_PULSE_LENGTH 2  // Time to hold Phi2 low (in milliseconds)
#define STEP_MSG_DURATION 50 // Time to display "...." (in milliseconds)

// Display Elements

// Default I2C Address and Brightness for HT16K33 Interface
#define DISPLAY_ADDRESS 0x70
#define MAX_BRIGHTNESS 15
#define MIN_BRIGHTNESS 0      // This is not "OFF"; .clear() for no display
#define DEFAULT_BRIGHTNESS 8
#define BLINK_RATE 1
#define MSG_DURATION 1000     // Time to show status messsage (milliseconds)

// Status Labels
#define STEP_LABEL      "STEP"
#define RUN_LABEL       "RUN "
#define STOP_LABEL      "STOP"
#define CLOCK_LABEL     "CLCK"
#define RESET_LABEL     "RSET"
#define SAVE_LABEL      "SAVE"
#define RESET_ON_LABEL  "RsON"
#define RESET_OFF_LABEL "RsOF"
#define BRIGHTNESS_FORMAT "Br%2u "

// Create the display instance and buffer
Adafruit_AlphaNum4 disp = Adafruit_AlphaNum4();
char display_buffer[4];
uint8_t brightness = DEFAULT_BRIGHTNESS;

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

// Frequency and Band entries are linked lists stored in an array.  The linked
// list allows easy iteration (it is always item = item->next), while the array
// "container" allows direct, indexed, access for save/load functionality.

// Adding a new Band or Frequency requires adding a new item in the array, with
// appropriate values, and adjusting the index and next_* accordingly.

// Frequency - Represents ONE Clock Frequency, as a node in a linked list.  The
// last entry in the list MUST point back to the first!
struct Frequency {  
  uint8_t index;             // Position in array, for Save/Load lookup
  String display_text;       // Text value to display
  uint8_t tccr1b_flags;      // ATMega328 TCCR1B Timer Control Register - Mode & Prescale Values
  uint16_t timer_count;      // ATMega328 CTC Mode Timer Match value (OCR1A)
  Frequency* next_frequency; // Pointer to next Frequency entry
};

// Band (Frequency) - Reprents ONE Frequency Band, as a node in a linked list.
// The last entry in the list MUST point back to the first!
struct Band {
  uint8_t index;            // Position in array, for Save/Load lookup
  String display_text;      // Text value to display
  Frequency *frequencyBand; // Pointer to the Frequency Array for this Band
  Band *nextBand;           // Pointer to the next Band entry
};

// Adding a new Frequency just requires choosing the appropriate prescaler
// (tccr1b_flags) and timer_count values, adding it to the array, and adjusting
// the indexes and next_* pointers accordingly.
Frequency hz_frequencies[] = {
  {0, " 1Hz", CTS_PRESCALER_256, 0x79B8, &hz_frequencies[1]},
  {1, " 2Hz", CTC_PRESCALER_64,  0xF370, &hz_frequencies[2]},
  {2, " 5Hz", CTC_PRESCALER_64,  0x615D, &hz_frequencies[3]},
  {3, "10Hz", CTC_PRESCALER_64,  0x30B0, &hz_frequencies[4]},
  {4, "25Hz", CTC_PRESCALER_8,   0x9BCE, &hz_frequencies[5]},
  {5, "50Hz", CTC_PRESCALER_8,   0x4DE4, &hz_frequencies[6]},
  {6, "100H", CTC_PRESCALER_8,   0x2700, &hz_frequencies[7]},
  {7, "250H", CTC_PRESCALER_OFF, 0x7CA4, &hz_frequencies[8]},
  {8, "500H", CTC_PRESCALER_OFF, 0x3E51, &hz_frequencies[0]},
};

Frequency khz_frequencies[] = {
  {0, "  1k", CTC_PRESCALER_OFF, 0x1F28, &khz_frequencies[1]},
  {1, "  2k", CTC_PRESCALER_OFF, 0x0FA0, &khz_frequencies[2]},
  {2, "  5k", CTC_PRESCALER_OFF, 0x063A, &khz_frequencies[3]},
  {3, " 10k", CTC_PRESCALER_OFF, 0x031D, &khz_frequencies[4]},
  {4, " 25k", CTC_PRESCALER_OFF, 0x013E, &khz_frequencies[5]},
  {5, " 50k", CTC_PRESCALER_OFF, 0x009F, &khz_frequencies[6]},
  {6, "100k", CTC_PRESCALER_OFF, 0x004E, &khz_frequencies[7]},
  {7, "250k", CTC_PRESCALER_OFF, 0x001F, &khz_frequencies[8]},
  {8, "500k", CTC_PRESCALER_OFF, 0x000F, &khz_frequencies[0]},
};

Frequency mhz_frequencies[] = {
  {0, "1MHz", CTC_PRESCALER_OFF, 0x0007, &mhz_frequencies[1]},
  {1, "2MHz", CTC_PRESCALER_OFF, 0x0003, &mhz_frequencies[2]},
  {2, "4MHz", CTC_PRESCALER_OFF, 0x0001, &mhz_frequencies[3]},
  {3, "8MHz", CTC_PRESCALER_OFF, 0x0000, &mhz_frequencies[0]}, 
};

// Adding a new Band just requires creating a supporting frequencies list/array,
// and adding the new band to the bands[] array, adjusting the indexes and
// next_* pointers accordingly.
Band bands[] = {
  {0, "  Hz", hz_frequencies,  &bands[1]},
  {1, " kHz", khz_frequencies, &bands[2]},
  {2, " MHz", mhz_frequencies, &bands[0]}
};

// We want the first Frequency of the first Band ...
Band *b = &bands[0];
Frequency *f = b->frequencyBand;

// Create input buttons
EasyButton stepButton(STEP_BUTTON);
EasyButton freqButton(FREQ_BUTTON);
EasyButton resetButton(RESET_BUTTON);
EasyButton saveButton(SAVE_BUTTON);

// Clock Mode
enum Modes { SINGLE, CLOCK };
uint8_t mode =  Modes::CLOCK;

// Clock Run State
enum States { RUN, STOP };
uint8_t state = States::STOP;

// Reset on Start
enum Reset { ENABLE, DISABLE };
uint8_t reset = Reset::DISABLE;

// EEPROM storage 
#define EEPROM_SENTINEL 0x2A // Unlikely value; indicates presence of saved data

struct SavedData {
  uint8_t sentinel;
  Modes mode;
  States state;
  Reset reset;
  uint8_t band;
  uint8_t frequency;
  uint8_t brightness;
};

// Utility Functions

// Abstraction for delay; to allow potential future change
// to a non-blocking, preemptable, implementation.
void wait(uint32_t milliseconds) {
  delay(milliseconds);
}

// Core Methods

// Write a string to the LED display
void displayLED(String str, bool show_dot = false) {  
  for(uint8_t i = 0; i <= 3; i++){
    disp.writeDigitAscii(i, str[i], show_dot);
  }
  disp.writeDisplay();
}

// Sets the new clock frequency and updates the display
void setFrequency(Frequency* frequency) {
  // Disable the Timer's OUTPUT
  TCCR1A = OC1A_OFF;

  // Display the Frequency
  displayLED(frequency->display_text);

  // Set the Prescaler and Counter Mode
  TCCR1B = frequency->tccr1b_flags;
  OCR1A  = frequency->timer_count;

  // Enable the Timer's OUTPUT (Toggle Mode)
  if( state == States::RUN) {
    TCCR1A = OC1A_TOGGLE;
    disp.blinkRate(BLINK_RATE);
  }
}

// Perform the RESET line toggle
void doReset() {
  // Perform the RESET
  digitalWrite(RESET_PIN, LOW);
  // This is deliberately blocking!
  delay(RESET_DURATION);
  digitalWrite(RESET_PIN, HIGH);
}

// Update the display state, resetting the clock and enabling
// RUN mode as necessary
void updateState(uint16_t milliseconds) {
  wait(milliseconds);
  // Put the display back in the proper state
  if (mode == Modes::CLOCK) {
    setFrequency(f);    
  } else {
    displayLED(STEP_LABEL);
  }
}
// Load STARTUP state from EEPROM
void Load() {
  // Read EEPROM data
  SavedData data;
  EEPROM.get(0, data);
  // Valid save?
  if (data.sentinel != EEPROM_SENTINEL) return;

  // Valid save, so restore state!
  brightness = data.brightness;
  disp.setBrightness(brightness);
  if (data.reset == Reset::ENABLE) doReset();
  b = &bands[data.band];
  f = &b->frequencyBand[data.frequency];
  mode = data.mode;
  state = data.state;
  reset = data.reset;

  // Update the display and blink rate, starting the clock
  // if necessary
  updateState(0);
}

// Button Event Handlers

// Issue a STEP pulse OR toggle between RUN and STOP modes
void onPressedStep() {
  // Toggle CLOCK/STEP modes
  if (mode == Modes::CLOCK) {
    // Toggle RUN/STOP state
    if (state == States::STOP) {
      state = States::RUN;
      displayLED(RUN_LABEL);
      wait(MSG_DURATION);
      setFrequency(f);            
    } else {
      state = States::STOP;
      TCCR1A = OC1A_OFF;
      displayLED(STOP_LABEL);
      disp.blinkRate(0);
      wait(MSG_DURATION);
      displayLED(f->display_text);        
    }
  } else {
    // STEP
    displayLED(STEP_LABEL, true);
    // Emit a LOW pulse
    digitalWrite(CLOCK_PIN, LOW);
    // This is deliberately blocking!
    delay(STEP_PULSE_LENGTH);
    digitalWrite(CLOCK_PIN, HIGH);
    // Keep the "...." displayed long enough to be visible
    // without meaningfully slowing down STEP actions
    wait(STEP_MSG_DURATION);
    displayLED(STEP_LABEL, false);
  }
}

// Next Frequency in current band
void onPressedFreq() {
  f = f->next_frequency;
  setFrequency(f);  
}

// Next Frequency Band
void onLongPressFreq() {
  disp.blinkRate(0);  
  b = b->nextBand;
  f = &b->frequencyBand[0];
  displayLED(b->display_text);
  wait(MSG_DURATION);
  setFrequency(f); 
}

// Switch between Single STEP and CLOCK modes
void onLongPressStep() {
  disp.blinkRate(0);
  if (mode == Modes::SINGLE) {
    mode = Modes::CLOCK;
    displayLED(CLOCK_LABEL);
    updateState(MSG_DURATION);
  } else {
    mode = Modes::SINGLE;
    digitalWrite(CLOCK_PIN, HIGH);
    displayLED(STEP_LABEL);
    state = States::STOP;
    TCCR1A = OC1A_OFF;
  }  
}

// Cycle the RESET pin LOW, then HIGH
void onPressedReset() {
  // Show operation
  displayLED(RESET_LABEL);
  // Disable blinking if running
  if (state == States::RUN) disp.blinkRate(0);
  // Perform the RESET
  doReset();
  updateState(MSG_DURATION);
  // Revert display
  //wait(MSG_DURATION);
  //displayLED(f->display_text);
  //if (state == States::RUN) disp.blinkRate(BLINK_RATE);
}

// Toggles RESET on Start up flag
void onLongPressReset() {
  disp.blinkRate(0);
  if (reset == Reset::ENABLE) {
    reset = Reset::DISABLE;
    displayLED(RESET_ON_LABEL);
  } else {
    reset = Reset::ENABLE;
    displayLED(RESET_OFF_LABEL);
  }

  updateState(MSG_DURATION);  
}

// Short press on SAVE changes brightness
void onPressedSave() {
  // Disable blinking if running
  if (state == States::RUN) disp.blinkRate(0);
  brightness++;
  if (brightness > MAX_BRIGHTNESS ) brightness = MIN_BRIGHTNESS;
  disp.setBrightness(brightness);
  sprintf(display_buffer, BRIGHTNESS_FORMAT, brightness);
  displayLED((String)display_buffer); 

  updateState(MSG_DURATION / 2);
}

// Save State to EEPROM
void onLongPressSave() {
  // Disable blinking if running
  if (state == States::RUN) disp.blinkRate(0);
  // Display operation 
  displayLED(SAVE_LABEL);
  // Get the current state and build a save structure 
  SavedData data;
  data.sentinel = EEPROM_SENTINEL;
  data.mode = mode;
  data.state = state;
  data.reset = reset;
  data.brightness = brightness;
  data.band = b->index;
  data.frequency = f->index;
  // Store the data at the start of the EEPROM's memory
  EEPROM.put(0, data);
  
  updateState(MSG_DURATION);
}

// Initialize pins, display and startup state
void setup() {

  // When not operating as OC1A, PIN 9 (Clock) is an OUTPUT
  pinMode(CLOCK_PIN, OUTPUT);

  // RESB (Reset) PIN is HIGH in normal operation
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);
  
  // Setup the LED Display
  disp.begin(DISPLAY_ADDRESS);
  disp.setBrightness(brightness);
  disp.clear();
  disp.writeDisplay();

  // Setup the Button's event handlers
  stepButton.begin();
  stepButton.onPressed(onPressedStep);
  stepButton.onPressedFor(LONG_PRESS_DURATION, onLongPressStep);
  freqButton.begin();
  freqButton.onPressed(onPressedFreq);
  freqButton.onPressedFor(LONG_PRESS_DURATION, onLongPressFreq);  
  resetButton.begin();
  resetButton.onPressed(onPressedReset);
  resetButton.onPressedFor(LONG_PRESS_DURATION, onLongPressReset);
  saveButton.begin();
  saveButton.onPressed(onPressedSave);
  saveButton.onPressedFor(LONG_PRESS_DURATION, onLongPressSave);
  setFrequency(f);

  // Load and Apply saved state
  Load();
}

// Main loop - Everything is event driven basedon button presses
// while the CLOCK timer output runs in the background automatically
void loop() {
  // Read Buttons
  stepButton.read();
  freqButton.read();
  resetButton.read();
  saveButton.read();      
}
