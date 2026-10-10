// Nano-Clock - Arduino Nano based system clock generator
//
// Copyright (C) 2026, Ian Dunmore
//
// License: MIT (https://github.com/idunmore/phi2/blob/main/LICENSE)


// Core Components
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

// External Libraries
#include <EasyButton.h>

// Interface Definitions

// Button Pins
#define STEP_BUTTON 4
#define FREQ_BUTTON 5
#define RESET_BUTTON 6
#define SAVE_BUTTON 6 // TODO: Change to 7 once RESET is implemented!

// Long Press Duration (in milliseconds)
#define LONG_PRESS_DURATION 1000

// Output Pins
#define RESET_PIN 8
#define CLOCK_PIN 9

// Display Elements

// Default I2C Address and Brightness for HT16K33 Interface
#define DISPLAY_ADDRESS 0x70
#define MAX_BRIGHTNESS 15
#define MIN_BRIGHTNESS 0      // OFF, for "quiet" usage
#define DEFAULT_BRIGHTNESS 8
#define BLINK_RATE 1
#define MSG_DURATION 1000     // Time to show status messsage (milliseconds)

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
  {0, "  1H", CTS_PRESCALER_256, 0x79B8, &hz_frequencies[1]},
  {1, "  2H", CTC_PRESCALER_64,  0xF370, &hz_frequencies[2]},
  {2, "  5H", CTC_PRESCALER_64,  0x615D, &hz_frequencies[3]},
  {3, " 10H", CTC_PRESCALER_64,  0x30B0, &hz_frequencies[4]},
  {4, " 25H", CTC_PRESCALER_8,   0x9BCE, &hz_frequencies[5]},
  {5, " 50H", CTC_PRESCALER_8,   0x4DE4, &hz_frequencies[6]},
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
//EasyButton resetButton(RESET_BUTTON);
EasyButton saveButton(SAVE_BUTTON);

enum modes { SINGLE, CLOCK };
int mode =  modes::CLOCK;
enum state { RUN, STOP };
int state = state::STOP;

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
  TCCR1A = OC1A_TOGGLE;  
}

void onPressedStep() {
  // Toggle CLOCK/STEP modes
  if (mode == modes::CLOCK) {
    // Toggle RUN/STOP state
    if (state == state::STOP) {
      state = state::RUN;
      displayLED("RUN ");
      delay(MSG_DURATION);
      setFrequency(f);
      disp.blinkRate(BLINK_RATE);      
    } else {
      state = state::STOP;
      TCCR1A = OC1A_OFF;
      displayLED("STOP");
      disp.blinkRate(0);
      delay(MSG_DURATION);        
    }
  } else {
    // STEP
    displayLED("STEP", true);
    digitalWrite(CLOCK_PIN, LOW);
    delay(2);
    digitalWrite(CLOCK_PIN, HIGH);
    delay(50);
    displayLED("STEP", false);
  }
}

void onPressedFreq() {
  f = f->next_frequency;
  setFrequency(f);  
}

void onLongPressFreq() {  
  b = b->nextBand;
  f = &b->frequencyBand[0];
  displayLED(b->display_text);
  delay(MSG_DURATION);
  setFrequency(f); 
}

void onLongPressStep() {
  if (mode == modes::SINGLE) {
    mode = modes::CLOCK;
    displayLED("CLCK");
  } else {
    mode = modes::SINGLE;
    digitalWrite(9, HIGH);
    displayLED("STEP");
    state = state::STOP;
    TCCR1A = OC1A_OFF;
  }
}

// Short press on SAVE changes brightness
void onPressedSave() {
  brightness++;
  if (brightness > MAX_BRIGHTNESS ) brightness = MIN_BRIGHTNESS;
  disp.setBrightness(brightness);
  sprintf(display_buffer, "Br%2u ", brightness);
  displayLED((String)display_buffer);

  // TODO: In loop() track time since last press, so we can revert
  // to display the frequency!
}

void setup() {

  // When not operating as OC1A, PIN 9 (Clock) is an OUTPUT
  pinMode(CLOCK_PIN, OUTPUT);

  // TODO: Remove when implementation finished and tested
  // If debugging, uncomment
  // Serial.begin(57600);

  // Setup the LED Display
  disp.begin(DISPLAY_ADDRESS);
  disp.setBrightness(brightness);
  disp.clear();
  disp.writeDisplay();

  // Setup the Buttons
  stepButton.begin();
  stepButton.onPressed(onPressedStep);
  stepButton.onPressedFor(LONG_PRESS_DURATION, onLongPressStep);
  freqButton.begin();
  freqButton.onPressed(onPressedFreq);
  freqButton.onPressedFor(LONG_PRESS_DURATION, onLongPressFreq);
  saveButton.begin();
  saveButton.onPressed(onPressedSave);
  setFrequency(f);  
}

void loop() {
  // Read Buttons
  stepButton.read();
  freqButton.read();
  saveButton.read();

  // TODO: After DISP_RESET_DELAY expires, revert to the frequency display.  
}
