// Nano-Clock-OLED - Arduino Nano based system clock generator
//
// Copyright (C) 2026, Ian Dunmore
//
// License: MIT (https://github.com/idunmore/phi2/blob/main/LICENSE)

// Core Components
#include <Wire.h>
#include <EEPROM.h>

// External Libraries
#include <U8g2lib.h>
#include <EasyButton.h>

// Interface Definitions

// Screen dimensions (in pixels)
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Clock Waveform position and height
#define WAVE_HEIGHT 12
#define WAVE_VPOS   63

#define TITLE "Nano Clock OLED"
#define TITLE_VPOS  15

// Button Pins
#define STEP_BUTTON  2
#define FREQ_BUTTON  3
#define RESET_BUTTON 4
#define SAVE_BUTTON  5

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

// Default I2C Address for SSD1306 Interface
#define DISPLAY_ADDRESS     0x3C
#define BLINK_RATE          1
#define MSG_DURATION        1000  // Time to show status messsage (milliseconds)
#define NUM_CHARS           4     // Actual number of LED digits

#define I2C_SDA 4   // I2C SDA PIN (Nano Hardware I2C)
#define I2C_SCL 5   // I2C SCL PIN (Nano Hardware I2C)

// Status Labels
#define STEP_LABEL        "STEP"
#define RUN_LABEL         "RUN "
#define STOP_LABEL        "STOP"
#define CLOCK_LABEL       "CLOCK"
#define RESET_LABEL       "RESET"
#define SAVE_LABEL        "SAVE"
#define RESET_ON_LABEL    "RstON"  // Auto Reset at Power-Up ON
#define RESET_OFF_LABEL   "RstOFF" // Auto Reset at Power-Up OFF

// Create the display instance
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA);

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
  Frequency *frequency_band; // Pointer to the Frequency Array for this Band
  Band *next_band;           // Pointer to the next Band entry
};

// Adding a new Frequency just requires choosing the appropriate prescaler
// (tccr1b_flags) and timer_count values, adding it to the array, and adjusting
// the indexes and next_* pointers accordingly.
Frequency hz_frequencies[] = {
  {0, "1",    CTS_PRESCALER_256, 0x79B8, &hz_frequencies[1]},
  {1, "2",    CTC_PRESCALER_64,  0xF370, &hz_frequencies[2]},
  {2, "5",    CTC_PRESCALER_64,  0x615D, &hz_frequencies[3]},
  {3, "10",   CTC_PRESCALER_64,  0x30B0, &hz_frequencies[4]},
  {4, "25",   CTC_PRESCALER_8,   0x9BCE, &hz_frequencies[5]},
  {5, "50",   CTC_PRESCALER_8,   0x4DE4, &hz_frequencies[6]},
  {6, "100",  CTC_PRESCALER_8,   0x2700, &hz_frequencies[7]},
  {7, "250",  CTC_PRESCALER_OFF, 0x7CA4, &hz_frequencies[8]},
  {8, "500",  CTC_PRESCALER_OFF, 0x3E51, &hz_frequencies[0]},
};

Frequency khz_frequencies[] = {
  {0, "1",    CTC_PRESCALER_OFF, 0x1F28, &khz_frequencies[1]},
  {1, "2",    CTC_PRESCALER_OFF, 0x0FA0, &khz_frequencies[2]},
  {2, "5",    CTC_PRESCALER_OFF, 0x063A, &khz_frequencies[3]},
  {3, "10",   CTC_PRESCALER_OFF, 0x031D, &khz_frequencies[4]},
  {4, "25",   CTC_PRESCALER_OFF, 0x013E, &khz_frequencies[5]},
  {5, "50",   CTC_PRESCALER_OFF, 0x009F, &khz_frequencies[6]},
  {6, "100",  CTC_PRESCALER_OFF, 0x004E, &khz_frequencies[7]},
  {7, "250",  CTC_PRESCALER_OFF, 0x001F, &khz_frequencies[8]},
  {8, "500",  CTC_PRESCALER_OFF, 0x000F, &khz_frequencies[0]},
};

Frequency mhz_frequencies[] = {
  {0, "1", CTC_PRESCALER_OFF, 0x0007, &mhz_frequencies[1]},
  {1, "2", CTC_PRESCALER_OFF, 0x0003, &mhz_frequencies[2]},
  {2, "4", CTC_PRESCALER_OFF, 0x0001, &mhz_frequencies[3]},
  {3, "8", CTC_PRESCALER_OFF, 0x0000, &mhz_frequencies[0]}, 
};

// Adding a new Band just requires creating a supporting frequencies list/array,
// and adding the new band to the bands[] array, adjusting the indexes and
// next_* pointers accordingly.
Band bands[] = {
  {0, "Hz", hz_frequencies,  &bands[1]},
  {1, "kHz", khz_frequencies, &bands[2]},
  {2, "MHz", mhz_frequencies, &bands[0]}
};

// Create input buttons
EasyButton stepButton(STEP_BUTTON);
EasyButton freqButton(FREQ_BUTTON);
EasyButton resetButton(RESET_BUTTON);
EasyButton saveButton(SAVE_BUTTON);

// Clock Mode
enum Modes { SINGLE, CLOCK };

// Clock Run State
enum States { RUN, STOP };

// Reset on Start
enum Reset { ENABLE, DISABLE };

// EEPROM storage
#define EEPROM_SENTINEL 0x2A // Unlikely value; indicates presence of saved data

// Clock State - The current state of the clock, which is also the record that
// is saved to, and loaded from, EEPROM.  Field order defines the EEPROM layout.
struct ClockState {
  uint8_t sentinel;     // EEPROM_SENTINEL when saved; indicates a valid save
  Modes mode;
  States state;
  Reset reset;
  uint8_t band;         // Index into bands[]
  uint8_t frequency;    // Index into the current Band's frequency array
  bool display_enable;  // Display is enabled or disabled
  
  Band* currentBand() { return &bands[band]; }
  Frequency* currentFrequency() { return &bands[band].frequency_band[frequency]; }

  void Load();
  void Save();
};

// Default Clock State for first run (will be overwritten if any values were saved)
ClockState clock_state = {
  0, Modes::CLOCK, States::STOP, Reset::DISABLE, 0, 0, true
};

// Waveform Scroll Position
uint8_t waveform_scroll = 0;

// Utility Functions

// Abstraction for delay; to allow potential future change
// to a non-blocking, preemptable, implementation.
void wait(uint32_t milliseconds) {
  delay(milliseconds);
}

// Core Methods

void displayStatusLine() {
  String str = TITLE;
  u8g2.setFont(u8g2_font_tenthinguys_tr);
  uint8_t x_pos = (SCREEN_WIDTH - u8g2.getStrWidth(str.c_str())) / 2;
  u8g2.drawStr(x_pos, TITLE_VPOS, str.c_str());
}

// Write a string to the OLED display
void displayOLED(String str) {
  u8g2.firstPage();
  do {   
    displayStatusLine();  
    u8g2.setFont(u8g2_font_calblk36_tr);
    uint8_t text_width = u8g2.getStrWidth(str.c_str());    
    u8g2.drawStr((SCREEN_WIDTH - text_width)/2,45,str.c_str());
    drawWaveForm(16, waveform_scroll);    
  } while ( u8g2.nextPage() );  
}

// Sets the new clock frequency and updates the display
void setFrequency(Band* band, Frequency* frequency) {
  // Disable the Timer's OUTPUT
  TCCR1A = OC1A_OFF;

  // Create the frequency display text
  String frequency_text = frequency->display_text + band->display_text;

  // Display the Frequency
  displayStatusLine();
  displayOLED(frequency_text);

  // Set the Prescaler and Counter Mode
  TCCR1B = frequency->tccr1b_flags;
  OCR1A  = frequency->timer_count;

  // Enable the Timer's OUTPUT (Toggle Mode)
  if( clock_state.state == States::RUN) {
    TCCR1A = OC1A_TOGGLE;
    //disp.blinkRate(BLINK_RATE);
  }
}

// Draws a square wave across the full screen width, always starting at X=0.
// "cycle" is the width (in pixels) of each peak/trough; "offset" is how many
// pixels into the waveform the drawing begins (i.e. its phase).
void drawWaveForm(uint8_t cycle, uint8_t offset) {
  if (cycle == 0) return;

  // One full period is a trough followed by a peak, so only the offset
  // modulo the period matters.
  uint8_t phase = offset % (cycle * 2);
  bool peak = (phase >= cycle);

  // The segment containing X=0 started "phase % cycle" pixels off-screen to
  // the left. Use a signed type so it can be negative, and clip to X=0 when
  // drawing (u8g2 coordinates are unsigned).
  for (int16_t wave_x = -(int16_t)(phase % cycle); wave_x < SCREEN_WIDTH; wave_x += cycle) {
    int16_t start = (wave_x < 0) ? 0 : wave_x;
    int16_t edge  = wave_x + cycle;   // X of the rising/falling edge ending this segment

    u8g2.drawHLine(start, peak ? WAVE_VPOS - WAVE_HEIGHT : WAVE_VPOS, edge - start + 1);
    if (edge < SCREEN_WIDTH) {
      u8g2.drawVLine(edge, WAVE_VPOS - WAVE_HEIGHT, WAVE_HEIGHT);
    }
    peak = !peak;
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
  if (clock_state.mode == Modes::CLOCK) {
    setFrequency(clock_state.currentBand(), clock_state.currentFrequency());
  } else {
    displayOLED(STEP_LABEL);
  }
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
  //disp.setBrightness(brightness);
  if (reset == Reset::ENABLE) doReset();

  // Update the display and blink rate, starting the clock
  // if necessary
  updateState(0);
}

// Save state to EEPROM
void ClockState::Save() {
  sentinel = EEPROM_SENTINEL;
  // Store the data at the start of the EEPROM's memory
  EEPROM.put(0, *this);
}

// Button Event Handlers

// Issue a STEP pulse OR toggle between RUN and STOP modes
void onPressedStep() {
  // Toggle CLOCK/STEP modes
  if (clock_state.mode == Modes::CLOCK) {
    // Toggle RUN/STOP state
    if (clock_state.state == States::STOP) {
      clock_state.state = States::RUN;
      displayOLED(RUN_LABEL);
      wait(MSG_DURATION);
      setFrequency(clock_state.currentBand(), clock_state.currentFrequency());
    } else {
      clock_state.state = States::STOP;
      TCCR1A = OC1A_OFF;
      displayOLED(STOP_LABEL);
      //disp.blinkRate(0);
      wait(MSG_DURATION);
      displayOLED(clock_state.currentFrequency()->display_text);
    }
  } else {
    // STEP
    displayOLED(STEP_LABEL);
    // Emit a LOW pulse
    digitalWrite(CLOCK_PIN, LOW);
    // This is deliberately blocking!
    delay(STEP_PULSE_LENGTH);
    digitalWrite(CLOCK_PIN, HIGH);
    // Keep the "...." displayed long enough to be visible
    // without meaningfully slowing down STEP actions
    wait(STEP_MSG_DURATION);
    displayOLED(STEP_LABEL);
  }
}

// Switch between Single STEP and CLOCK modes
void onLongPressStep() {
  //disp.blinkRate(0);
  if (clock_state.mode == Modes::SINGLE) {
    clock_state.mode = Modes::CLOCK;
    displayOLED(CLOCK_LABEL);
    updateState(MSG_DURATION);
  } else {
    clock_state.mode = Modes::SINGLE;
    digitalWrite(CLOCK_PIN, HIGH);
    displayOLED(STEP_LABEL);
    clock_state.state = States::STOP;
    TCCR1A = OC1A_OFF;
  }  
}

// Next Frequency in current band
void onPressedFreq() {
  clock_state.frequency = clock_state.currentFrequency()->next_frequency->index;
  setFrequency(clock_state.currentBand(), clock_state.currentFrequency());
}

// Next Frequency Band
void onLongPressFreq() {
  //disp.blinkRate(0);
  clock_state.band = clock_state.currentBand()->next_band->index;
  clock_state.frequency = 0;
  displayOLED(clock_state.currentBand()->display_text);
  wait(MSG_DURATION);
  setFrequency(clock_state.currentBand(), clock_state.currentFrequency());
}

// Cycle the RESET pin LOW, then HIGH
void onPressedReset() {
  // Show operation
  displayOLED(RESET_LABEL);
  // Disable blinking if running
  if (clock_state.state == States::RUN) ;//disp.blinkRate(0);
  // Perform the RESET
  doReset();
  updateState(MSG_DURATION);  
}

// Toggles RESET on Start up flag
void onLongPressReset() {
  //disp.blinkRate(0);
  if (clock_state.reset == Reset::ENABLE) {
    clock_state.reset = Reset::DISABLE;
    displayOLED(RESET_OFF_LABEL);
  } else {
    clock_state.reset = Reset::ENABLE;
    displayOLED(RESET_ON_LABEL);
  }
  updateState(MSG_DURATION);  
}

// Short press on SAVE toggles display on/off
void onPressedSave() {
  if (clock_state.display_enable) u8g2.clearDisplay();
  clock_state.display_enable = !clock_state.display_enable;  
  //sprintf(display_buffer, BRIGHTNESS_FORMAT, clock_state.brightness);
  //displayLED((String)display_buffer);
  //updateState(MSG_DURATION / 2);
}

// Save State to EEPROM
void onLongPressSave() {
  // Disable blinking if running
  if (clock_state.state == States::RUN) ;//disp.blinkRate(0);
  // Display operation
  displayOLED(SAVE_LABEL);
  clock_state.Save();
  updateState(MSG_DURATION);
}

// Initialize pins, display and startup state
void setup() {
  // When not operating as OC1A, PIN 9 (Clock) is an OUTPUT
  pinMode(CLOCK_PIN, OUTPUT);

  // RESB (Reset) PIN is HIGH in normal operation
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);  
  
  // Initialize display processing
  u8g2.begin();

  // Setup the Buttons' event handlers
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
  setFrequency(clock_state.currentBand(), clock_state.currentFrequency());

  // Load and Apply saved state
  clock_state.Load();
}

// Main loop - Everything is event driven basedon button presses
// while the CLOCK timer output runs in the background automatically
void loop() {
  // Read Buttons
  stepButton.read();
  freqButton.read();
  resetButton.read();
  saveButton.read();    

  // If the display is disabled, ensure it is clear and do nothing else
  if (!clock_state.display_enable) {
    u8g2.clearDisplay();
    return;    
  }
  
  // Display enabled
  u8g2.firstPage();
  do {
    displayStatusLine();

    drawWaveForm(16, waveform_scroll);  
  } while ( u8g2.nextPage() );  
  if (clock_state.state == States::RUN ) waveform_scroll+=1;  
}
