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

// Fonts (only the glyphs this sketch draws; if new text or icons need more
// glyphs, add them to ./tools/subset_fonts.py and re-run it to regenerate this file)
#include "clock_fonts.h"

// Interface Definitions

// Screen dimensions (in pixels)
#define SCREEN_WIDTH  128

// Output Strings, Sizes and Positions
#define WAVE_HEIGHT 12
#define WAVE_VPOS   63
#define WAVE_WIDTH  16

#define FREQ_VPOS   45
#define STATUS_VPOS 16

#define STEP_VPOS   32
#define STEP_HPOS   32
#define STEP_RADIUS 16
#define STEP_LENGTH 48

// How fast the heart-beat icon "beats";
// Lower is faster, 0 disables
#define HEART_BEAT_RATE 1

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

// State Transition/Message Durations
#define RESET_DURATION    2     // Time to hold RESB low (in milliseconds)
#define STEP_PULSE_LENGTH 2     // Time to hold Phi2 low (in milliseconds)
#define STEP_MSG_DURATION 100   // Time to display "...." (in milliseconds)
#define MSG_DURATION      1000  // Time to show status message (milliseconds)

// Display Elements

// Maximum length of a status message (excluding NUL terminator)
#define MESSAGE_MAX_LEN 6

// Create the display instance
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0);

// Timer Values & Structures

// Timer 1 - Prescaler (Clock Divider) and Timer Mode Values

// Bits 0-2 set the Pre-Scaler Value (RAW Clock Divider)
// Bit 3 sets CTC (Clear Timer on Compare match mode)
#define CTC_PRESCALER_OFF 0b00001001
#define CTC_PRESCALER_8   0b00001010
#define CTC_PRESCALER_64  0b00001011
#define CTC_PRESCALER_256 0b00001100

// TCCR1A - Control Values
#define OC1A_TOGGLE 0b01000000  // Toggle OC1A (Pin 9) on CTC Match
#define OC1A_OFF    0b00000000  // Disable OC1A/OC1B Timer output (Pin 9)

// Structures

// All Frequency and Band tables live in flash (PROGMEM) to save RAM, so their
// fields MUST be read with the pgm_read_*() / *_P() functions, never directly.

// Maximum length of display text (excluding NUL terminator) for table entries
#define TABLE_TEXT_LEN 3

// Frequency - Represents ONE Clock Frequency
struct Frequency {
  char display_text[TABLE_TEXT_LEN + 1];  // Text value to display
  uint8_t tccr1b_flags;                   // TCCR1B Timer Control Register - Mode & Prescale Values
  uint16_t timer_count;                   // CTC Mode Timer Match value (OCR1A)
};

// Band - Represents ONE Frequency Band, and the array of Frequencies within it
struct Band {
  char display_text[TABLE_TEXT_LEN + 1];  // Text value to display
  const Frequency* frequencies;           // Frequency array for this Band (PROGMEM)
  uint8_t frequency_count;                // Number of entries in that array
};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

// Adding a new Frequency just requires choosing the appropriate prescaler
// (tccr1b_flags) and timer_count values, and adding it to the array.
const Frequency hz_frequencies[] PROGMEM = {
  {"1",   CTC_PRESCALER_256, 0x79B8},
  {"2",   CTC_PRESCALER_64,  0xF370},
  {"5",   CTC_PRESCALER_64,  0x615D},
  {"10",  CTC_PRESCALER_64,  0x30B0},
  {"25",  CTC_PRESCALER_8,   0x9BCE},
  {"50",  CTC_PRESCALER_8,   0x4DE4},
  {"100", CTC_PRESCALER_8,   0x2700},
  {"250", CTC_PRESCALER_OFF, 0x7CA4},
  {"500", CTC_PRESCALER_OFF, 0x3E51},
};

const Frequency khz_frequencies[] PROGMEM = {
  {"1",   CTC_PRESCALER_OFF, 0x1F28},
  {"2",   CTC_PRESCALER_OFF, 0x0FA0},
  {"5",   CTC_PRESCALER_OFF, 0x063A},
  {"10",  CTC_PRESCALER_OFF, 0x031D},
  {"25",  CTC_PRESCALER_OFF, 0x013E},
  {"50",  CTC_PRESCALER_OFF, 0x009F},
  {"100", CTC_PRESCALER_OFF, 0x004E},
  {"250", CTC_PRESCALER_OFF, 0x001F},
  {"500", CTC_PRESCALER_OFF, 0x000F},
};

const Frequency mhz_frequencies[] PROGMEM = {
  {"1",   CTC_PRESCALER_OFF, 0x0007},
  {"2",   CTC_PRESCALER_OFF, 0x0003},
  {"4",   CTC_PRESCALER_OFF, 0x0001},
  {"8",   CTC_PRESCALER_OFF, 0x0000},
};

// Adding a new Band just requires creating a supporting frequencies array, and
// adding the new band to the bands[] array.
const Band bands[] PROGMEM = {
  {"Hz",  hz_frequencies,  COUNT_OF(hz_frequencies)},
  {"kHz", khz_frequencies, COUNT_OF(khz_frequencies)},
  {"MHz", mhz_frequencies, COUNT_OF(mhz_frequencies)},
};

// Create input buttons
EasyButton stepButton(STEP_BUTTON);
EasyButton freqButton(FREQ_BUTTON);
EasyButton resetButton(RESET_BUTTON);
EasyButton saveButton(SAVE_BUTTON);

// Clock Mode
enum Modes : uint8_t { SINGLE, CLOCK };

// Clock Run State
enum States : uint8_t { RUN, STOP };

// Reset on Start
enum Reset : uint8_t { ENABLE, DISABLE };

// EEPROM storage
#define EEPROM_SENTINEL 0x2B // Unlikely value; indicates presence of saved data

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
  
  // Both return pointers into PROGMEM; read fields with pgm_read_*() / *_P()
  const Band* currentBand() { return &bands[band]; }
  const Frequency* currentFrequency() {
    return (const Frequency*)pgm_read_ptr(&bands[band].frequencies) + frequency;
  }

  void Load();
  void Save();
};

// Default Clock State for first run (will be overwritten if any values were saved)
ClockState clock_state = {
  0, Modes::CLOCK, States::STOP, Reset::DISABLE, 0, 0, true
};

// Clock State Saved Flags
bool is_saved = false;
bool settings_changed = false;

// Waveform Scroll Position
uint8_t waveform_scroll = 0;

// Heart beat starts as 0 (values <127 show no heart), and if
// HEART_BEAT_RATE is non-zero, increments until it rolls over
uint8_t heart_beat = 0;

// Utility Functions

// Abstraction for delay; to allow potential future change
// to a non-blocking, preemptable, implementation.
void wait(uint32_t milliseconds) {
  delay(milliseconds);
}

// Core Methods

// Code Points for Status Glyphs
#define CLOCK_GLYPH     0x007b
#define STOP_GLYPH      0x00d9
#define RUN_GLYPH       0x00d3
#define STEP_GLYPH      0x00ee
#define RESET_GLYPH     0x00f3
#define SAVED_GLYPH     0x0081
#define UNCHANGED_GLYPH 0x0078
#define CHANGED_GLYPH   0x0079
#define HEART_GLYPH     0x00b7

// Draws the "status" line based on current state
void drawStatusLine() {
  // open_iconic_all_2x font (https://github.com/olikraus/u8g2/wiki/fntgrpiconic)
  u8g2.setFont(clock_icon_font);

  // Clock Mode and State
  if (clock_state.mode == Modes::CLOCK) {
    u8g2.drawGlyph(0, STATUS_VPOS, CLOCK_GLYPH);
    u8g2.drawGlyph(16, STATUS_VPOS, clock_state.state == States::RUN ? RUN_GLYPH : STOP_GLYPH);
  } else {
    u8g2.drawGlyph(30, STATUS_VPOS, STEP_GLYPH);
  }

  // Reset Status
  if (clock_state.reset == Reset::ENABLE) u8g2.drawGlyph(52, STATUS_VPOS, RESET_GLYPH);

  // Saved/Changed Settings Status
  if (is_saved) {
    u8g2.drawGlyph(72, STATUS_VPOS, SAVED_GLYPH);
    u8g2.drawGlyph(92, STATUS_VPOS, settings_changed ? CHANGED_GLYPH : UNCHANGED_GLYPH);
  }

  // Heart beat shows unit is running, NOT the clock pulses
  if (heart_beat > 127) {
    u8g2.drawGlyph(112, STATUS_VPOS, HEART_GLYPH);    
  }
  heart_beat+= HEART_BEAT_RATE;
}

// Draws text, horizontally centered, in the large font
void drawCenteredText(const char* text) {
  u8g2.setFont(clock_text_font);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth(text)) / 2, FREQ_VPOS, text);
}

// Draws a lit (on == true) or unlit "STEP" indicator with waveform "pulse"
void drawStepIndicator(bool on) {
  const uint8_t x = STEP_HPOS + STEP_RADIUS + 12;
  const uint8_t segment = STEP_LENGTH / 4;
  const uint8_t top = STEP_VPOS - STEP_RADIUS;

  if (!on) {
    u8g2.drawCircle(STEP_HPOS, STEP_VPOS, STEP_RADIUS);
    u8g2.drawHLine(x, STEP_VPOS, STEP_LENGTH);  // Flat-line
    return;
  }

  u8g2.drawDisc(STEP_HPOS, STEP_VPOS, STEP_RADIUS);
  // Down/up pulse (Phi2 on 6502 is active low)
  u8g2.drawHLine(x, STEP_VPOS, segment);
  u8g2.drawVLine(x + segment, STEP_VPOS, STEP_RADIUS);
  u8g2.drawHLine(x + segment, STEP_VPOS + STEP_RADIUS, segment + 1);
  u8g2.drawVLine(x + segment * 2, top, STEP_RADIUS * 2);
  u8g2.drawHLine(x + segment * 2, top, segment);
  u8g2.drawVLine(x + segment * 3, top, STEP_RADIUS);
  u8g2.drawHLine(x + segment * 3, STEP_VPOS, segment);
}

// Renders a complete frame: the status line, then text (or the step indicator
// if text is nullptr), then the waveform.  This is the only picture loop.
void drawScreen(const char* text, bool step_lit = false) {
  u8g2.firstPage();
  do {
    drawStatusLine();
    if (text) drawCenteredText(text);
    else drawStepIndicator(step_lit);
    drawWaveForm();
  } while (u8g2.nextPage());
}

// Shows a message stored in flash (PROGMEM), e.g. displayMessage(PSTR("Run"))
void displayMessage(const char* message) {
  char text[MESSAGE_MAX_LEN + 1];
  strcpy_P(text, message);
  drawScreen(text);
}

// Draws a square wave across the full screen width, always starting at X=0.
// Each peak/trough is narrower at higher bands (must stay a multiple of 2, or
// scrolling stutters); waveform_scroll is the phase of the wave, in pixels.
void drawWaveForm() {
  uint8_t cycle = WAVE_WIDTH >> clock_state.band;

  // Don't draw the wave form if we're not in clock mode or cycle length is 0
  if (cycle == 0 || clock_state.mode == Modes::SINGLE) return;

  // One full period is a trough followed by a peak, so only the offset
  // modulo the period matters.
  uint8_t phase = waveform_scroll % (cycle * 2);
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

// Sets the new clock frequency
void setFrequency(const Frequency* frequency) {
  // Disable the Timer's OUTPUT
  TCCR1A = OC1A_OFF;  

  // Set the Prescaler and Counter Mode
  TCCR1B = pgm_read_byte(&frequency->tccr1b_flags);
  OCR1A  = pgm_read_word(&frequency->timer_count);

  // Enable the Timer's OUTPUT (Toggle Mode)
  if( clock_state.state == States::RUN) {
    TCCR1A = OC1A_TOGGLE;    
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

// Waits for a message to be read, then re-applies the
// clock settings (which restarts the output, if running)
void resumeClockAfter(uint16_t milliseconds) {
  wait(milliseconds);
  if (clock_state.mode == Modes::CLOCK) {
    setFrequency(clock_state.currentFrequency());
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
  is_saved = true;
  *this = data;

  // Single-step mode idles with the clock HIGH, so that the first STEP
  // produces a full LOW pulse
  if (mode == Modes::SINGLE) digitalWrite(CLOCK_PIN, HIGH);

  if (reset == Reset::ENABLE) doReset();

  // Update the display and start the clock if necessary
  resumeClockAfter(0);
}

// Save state to EEPROM
void ClockState::Save() {
  sentinel = EEPROM_SENTINEL;
  // Store the data at the start of the EEPROM's memory
  EEPROM.put(0, *this);
  // Set saved state flags
  is_saved = true;
  settings_changed = false;
}

// Button Event Handlers

// Issue a STEP pulse OR toggle between RUN and STOP modes
void onPressedStep() {
  // Toggle CLOCK/STEP modes
  if (clock_state.mode == Modes::CLOCK) {
    // Saveable state changed
    settings_changed = true;

    // Toggle RUN/STOP state
    if (clock_state.state == States::STOP) {
      clock_state.state = States::RUN;
      displayMessage(PSTR("Run"));
      resumeClockAfter(MSG_DURATION);      
    } else {
      clock_state.state = States::STOP;
      TCCR1A = OC1A_OFF;
      displayMessage(PSTR("Stop"));     
      wait(MSG_DURATION);      
    }
  } else {
    // STEP    
    drawScreen(nullptr, true);
    // Emit a LOW pulse
    digitalWrite(CLOCK_PIN, LOW);
    // This is deliberately blocking!
    delay(STEP_PULSE_LENGTH);
    digitalWrite(CLOCK_PIN, HIGH);
    // Keep the indicator "lit" long enough to be visible
    // without meaningfully slowing down STEP actions
    wait(STEP_MSG_DURATION);    
  }
}

// Switch between Single STEP and CLOCK modes
void onLongPressStep() {
  // Toggle STEP/CLOCK mode
  if (clock_state.mode == Modes::SINGLE) {
    clock_state.mode = Modes::CLOCK;
    displayMessage(PSTR("Clock"));
    resumeClockAfter(MSG_DURATION);
  } else {
    clock_state.mode = Modes::SINGLE;
    digitalWrite(CLOCK_PIN, HIGH);
    displayMessage(PSTR("Step"));
    clock_state.state = States::STOP;
    TCCR1A = OC1A_OFF;
    wait(MSG_DURATION);
  }
  // Saveable state changed
  settings_changed = true;    
}

// Next Frequency in current band
void onPressedFreq() {
  uint8_t count = pgm_read_byte(&clock_state.currentBand()->frequency_count);
  clock_state.frequency = (clock_state.frequency + 1) % count;
  setFrequency(clock_state.currentFrequency());

  // Saveable state changed
  settings_changed = true;  
}

// Next Frequency Band
void onLongPressFreq() {  
  clock_state.band = (clock_state.band + 1) % COUNT_OF(bands);
  clock_state.frequency = 0;
  displayMessage(clock_state.currentBand()->display_text);
  resumeClockAfter(MSG_DURATION);

  // Saveable state changed
  settings_changed = true;   
}

// Cycle the RESET pin LOW, then HIGH
void onPressedReset() {
  // Show operation
  displayMessage(PSTR("Reset"));
  // Perform the RESET
  doReset();
  resumeClockAfter(MSG_DURATION);  
}

// Toggles RESET on Start up flag
void onLongPressReset() {  
  if (clock_state.reset == Reset::ENABLE) {
    clock_state.reset = Reset::DISABLE;
    displayMessage(PSTR("RstOFF"));
  } else {
    clock_state.reset = Reset::ENABLE;
    displayMessage(PSTR("RstON"));
  }
  resumeClockAfter(MSG_DURATION); 

  // Saveable state changed
  settings_changed = true;   
}

// Short press on SAVE toggles display on/off
void onPressedSave() {
  clock_state.display_enable = !clock_state.display_enable;

   // Saveable state changed
  settings_changed = true;    
}

// Save State to EEPROM
void onLongPressSave() {  
  // Display operation
  displayMessage(PSTR("Save"));
  clock_state.Save();
  resumeClockAfter(MSG_DURATION);
}

// Attach a button's short and long press handlers
void initButton(EasyButton& button, void (*pressed)(), void (*long_pressed)()) {
  button.begin();
  button.onPressed(pressed);
  button.onPressedFor(LONG_PRESS_DURATION, long_pressed);
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
  initButton(stepButton, onPressedStep, onLongPressStep);
  initButton(freqButton, onPressedFreq, onLongPressFreq);
  initButton(resetButton, onPressedReset, onLongPressReset);
  initButton(saveButton, onPressedSave, onLongPressSave);
  setFrequency(clock_state.currentFrequency());

  // Load and Apply saved state
  clock_state.Load();
}

// Main loop - Everything is event driven based on button presses
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
  
  // Display enabled: show the frequency in clock mode, else the step indicator
  if (clock_state.mode == Modes::CLOCK) {
    // Room for frequency text + band text + NUL terminator
    char text[TABLE_TEXT_LEN * 2 + 1];
    strcpy_P(text, clock_state.currentFrequency()->display_text);
    strcat_P(text, clock_state.currentBand()->display_text);
    drawScreen(text);
  } else {
    drawScreen(nullptr);
  }

  // Animates the scrolling wave form if the clock is running  
  if (clock_state.state == States::RUN ) waveform_scroll+=1;   
}
