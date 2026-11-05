# nano-clock-oled - Tools

The following tool(s) were created to make some aspects of **nano-clock-oled** either easier to implement or to support various optimizations in that implementation:

## subset_fonts.py"

This creates a custom font definition that contains only those glyphs and characters actually _used_ by **nano-clock-oled**, rather than including the full set of code points/characters for the entirety of both of the source fonts required.

It is provided the various TEXT strings and ICONS (glyphs) used in **nano-clock-oled.ino**, determines the minimal set of required code points from the respective source fonts, extracts them byte-for-byte, and created a .h file with just those required elements in it.  This results in a saving of about 8kB of FLASH (PROGMEM) storage vs. including the full font definition for each.

**NOTE:** If you modify the text labels or glyphs in the main `nano-clock-oled.ino` file, you **must** add the appropriate entries to the ICONS and TEXT entries in `subset_fonts.py` file and regenerate `clock_fonts.h`, otherwise those characters/icons will not be displayed.

### Font Sources

The fonts used herein are included as part of the u8g2 package:

  - u8g2_font_open_iconic_all_2x_t - provides the glyphs/icon data (reduced from 5617 to 269 bytes)
  - u8g2_font_calblk36_tr - provides the text and numeric characters (reduced from 3312 to 974 bytes)

### Usage

`subset_fonts.py` is a Python 3.15 script, which can either be marked exectuable and run directly, or launched via `python3 subset_fonts.py`.  In either case, it should be run from the **tools**/ directory.  

For a default Arduino installation, with u8g2 installed via the Arduino IDE's library manager, it will find the source fonts automatically, but it accepts an optional command line argument with the path to those fonts.

The resultant output will be placed in the parent directory from where `subset_fonts.py` is run, and will be directly included by the main program source file.
