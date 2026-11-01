PIXO PET LIFE v4 - KAWAII BUZZER, VOLUME & SOUND SETTINGS
=========================================================
Compatible design: classic ESP32 38-pin + SSD1306 128x64 OLED +
TTP223 touch module + two tactile pushbuttons.
Extends your original PIXO_Pet_Life_Menu_v3.ino. The existing eye
animations, petting timing, birthdays, memories, Wi-Fi time sync,
brightness, night mode, wardrobe and factory reset stay in place.

CONTENTS
  PIXO_Pet_Life_Menu_v4_Buzzer.ino   Arduino source
  PIXO_Pet_Life_Menu_v4_Buzzer.txt   Same complete source
  PIXO_Pet_Life_Menu_v4_Buzzer_README.txt   This file
  PIXO_Buzzer_Wiring.png             Visual connection guide

IMPORTANT: GPIO32 IS ALREADY USED FOR YOUR TOUCH SENSOR!
-------------------------------------------------------
DO NOT connect the buzzer to GPIO32, even if you have seen that in
another project. Use GPIO27 for PIXO's new buzzer.

YOUR EXISTING WIRING (UNCHANGED)
--------------------------------
OLED SDA          -> GPIO25
OLED SCL          -> GPIO26
OLED power        -> 3V3 and GND (as your current module requires)
TTP223 OUT        -> GPIO32
TTP223 VCC/GND    -> 3V3/GND
Button A          -> GPIO18 and GND (INPUT_PULLUP)
Button B          -> GPIO19 and GND (INPUT_PULLUP)

NEW WIRING, TWO-PIN LOW-CURRENT PASSIVE PIEZO
----------------------------------------------
    ESP32 GPIO27 ------------------ (+) passive piezo
    ESP32 GND   ------------------- (-) passive piezo

All grounds are common. The BUZZER + pin is NOT tied to 3.3V in
this direct-drive passive-piezo arrangement. Keep wiring short.
Only connect directly if the part is a high-impedance passive piezo
that draws sufficiently little current for an ESP32 GPIO. If the
part has a coil, draws more than ~8 mA, or you don't know its
current, use a transistor driver instead.

OPTIONAL NPN TRANSISTOR DRIVER (FOR HIGHER-CURRENT BUZZERS)
----------------------------------------------------------
    ESP32 GPIO27 -> 1 kOhm resistor -> NPN transistor BASE
    ESP32 GND    -------------------> NPN transistor EMITTER
    3.3V         -------------------> BUZZER positive (+)
    BUZZER negative (-) ------------> NPN transistor COLLECTOR

This is a low-side driver, appropriate when PWM on/off of the buzzer
is required. Follow the component's rated voltage/current. A
magnetic/inductive buzzer may need a flyback diode; a bare passive
piezo normally does not. NEVER connect 5V directly to GPIO27.

WHICH TYPE DID YOU BUY?
-----------------------
* PASSIVE PIEZO (no built-in oscillator): can generate different
  pitched notes and a cute startup melody using PWM. The firmware
  is designed for THIS TYPE. A passive transducer won't make a
  sustained beep just by applying DC voltage.
* ACTIVE BUZZER (built-in oscillator): makes one steady beep with
  direct 3.3V DC power. Can beep rhythmically, but cannot reliably
  play melodies or offer true software volume control. For the
  requested feature, replace it with a passive piezo/transducer.
* 3-PIN MODULE: module VCC to 3.3V, GND to common ground, SIG to
  GPIO27 ONLY if its signal interface is compatible with 3.3V
  PWM; an active 3-pin module still cannot do multi-note tunes.

CONFIG AT THE TOP OF THE .INO
------------------------------
  WIFI_SSID / WIFI_PASSWORD             Your 2.4 GHz Wi-Fi
  PET_OWNER_NAME                         Your owner's name
  BUZZER_PIN = 27                        Free output pin
  BUZZER_ENABLED_DEFAULT = true         Initial default
  BUZZER_VOLUME_DEFAULT = 2             Initial level 0-4
  BUZZER_MAX_VOLUME = 4                  Five preset levels

Previously saved sound preferences override the compiled defaults.
Factory reset clears them along with the rest of PIXO's pet data.

MENU CONTROLS
--------------
Button A short: open menu / go back / cancel editing
Button A long:  return directly to PIXO face
Button B short: next item; on Sound Level, cycle 0..4 & PREVIEW
Button B long:  open item / save selected value
Both together:  same royal Easter egg as v3
Touch:          same petting/wake/calendar behaviour as v3

SETTINGS SUBMENU
----------------
  1 Brightness     (unchanged)
  2 Auto night     (unchanged)
  3 Buzzer         NEW; B short ON/OFF; B long SAVE
  4 Sound level    NEW; B short 0-4, hear a preview; B long SAVE
  5 Clock sync     (unchanged)
  6 Pet owner      (unchanged)
  7 Pet memory     (unchanged)
  8 Factory reset  (unchanged; ARM then hold B 3 seconds)

Sound level zero is silent even with the buzzer enabled. The PWM
settings change the strength/timbre of a passive piezo; they are
not calibrated volume percentages and cannot make all buzzer
modules equally quiet or loud. Start at level 1 or 2.

CUTE SOUND EVENTS
-----------------
Startup intro: an original short kawaii multi-note jingle, after
  Wi-Fi date/time sync completes and the PIXO logo intro begins.
Waking: 4 rising twinkles during shaking-eyes wake-up.
Drowsy: quiet descending sleepy/yawn tone.
Sleeping: an occasional very low, two-note dreamy purr every
  25-45 seconds (level capped to quiet even if daytime is loud).
Buttons: different little double-chirps for buttons A and B.
Petting: a tiny happy twinkle after a detected touch.
Headphones/music animation: original playful melody timed with the
  music expression (no audio file or copyrighted song).
Birthday: special celebratory arpeggio.
A+B secret: magical rising tones.

All sounds are sequenced WITHOUT delay(). Buzzer playback doesn't
pause the OLED animation, Wi-Fi handling, touch, menu, or clocks.
Sound ON/OFF and volume are stored in the existing 'pixo' NVS
namespace using separate keys 'sndon' and 'sndvol'; the existing
pet profile/birthday memory format has NOT changed.

ARDUINO IDE
------------
* Board: your classic ESP32 board (commonly ESP32 Dev Module).
* Install/use ESP32 boards package (Arduino-ESP32 2.x or 3.x).
* Libraries: Adafruit GFX and Adafruit SSD1306.
* WiFi, Wire, Preferences and LEDC come with the ESP32 board core.
* Open the .ino, enter credentials and owner name, connect GPIO27
  only after checking buzzer type/current, and upload.
* Serial Monitor: 115200 baud.

TROUBLESHOOTING
---------------
No sound: make sure Settings > Buzzer = ON, Sound Level > 0,
  GPIO27 connected, common GND present, and the buzzer is passive.
One monotonous beep instead of notes: likely ACTIVE buzzer; use a
  passive piezo for melodies.
Too harsh/loud: lower the Sound Level and/or add a driver/series
  resistance as appropriate for your piezo's electrical specs.
PIXO still pets but touching does not always retrigger: TTP223
  slow-release HIGH behavior is unchanged from v3.
OLED stops or ESP32 resets: check wiring, buzzer current, supply
  voltage and shared ground. Never drive a high-current buzzer
  directly from an ESP32 GPIO.

TESTING NOTE
-------------
The source is syntax-checked using simulated Arduino/ESP32 APIs for
both 2.x and 3.x LEDC paths and checked for menu/audio sequencing;
it has NOT been compiled on your exact ESP32 Arduino installation or
verified with your physical buzzer. Sound tone and loudness vary with
component type and build.
