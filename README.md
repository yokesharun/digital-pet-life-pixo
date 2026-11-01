PIXO PET LIFE v5 - WEATHER + SECRETS + FEEDING
=============================================
Board: Classic ESP32 38-pin development board
OLED: SSD1306 128 x 64 (Adafruit_GFX + Adafruit_SSD1306)
Date/time: Asia/Kolkata (IST, UTC+05:30, no DST)

CONTENTS
--------
- PIXO_Pet_Life_v5_Weather_Secrets.ino (upload using Arduino IDE)
- PIXO_Pet_Life_v5_Weather_Secrets.txt (same source, in text format)
- This README.

SETUP BEFORE UPLOAD
-------------------
Near the top of the .ino file, edit:
  WIFI_SSID              -> your 2.4 GHz Wi-Fi SSID
  WIFI_PASSWORD          -> your Wi-Fi password
  PET_OWNER_NAME         -> the human name PIXO remembers
  BUZZER_FIXED_HZ        -> defaults to 250 (suggested 200-300 Hz)
  TEST_MODE_DEFAULT      -> false (developer default)
  WEATHER_ENABLED        -> true

Also ensure the board package selected is a compatible classic ESP32 device.
Install Adafruit GFX Library and Adafruit SSD1306 through Arduino IDE.
WiFi, WiFiClientSecure, HTTPClient, Wire, time and Preferences are available
with the ESP32 Arduino board support package.

PIN WIRING - DO NOT CONNECT Buzzer to GPIO32
---------------------------------------------
OLED SDA             GPIO25
OLED SCL             GPIO26
OLED power           3.3V / GND (check module voltage rating)
Button A (Menu)      GPIO18 <-> switch <-> GND
Button B (Next/Feed) GPIO19 <-> switch <-> GND
Piezo buzzer (+)     GPIO27, only for a SMALL LOW-CURRENT PASSIVE PIEZO
Piezo buzzer (-)     Common GND
Old touch input     GPIO32 UNUSED / disconnected

Buttons are momentary 4-pin tactile switches. Connect one leg from each
ELECTRICAL SIDE of the switch, NOT two internally tied legs. Test with a
multimeter. Firmware uses INPUT_PULLUP; no added button resistors needed.
Wire everything with power OFF.

BUZZER IMPORTANT
----------------
A passive piezo transducer is needed for clean PWM at 250 Hz. There is NO
volume slider: the saved setting is Sounds ON/OFF. All effects use the SAME
250 Hz pitch with different beep durations and pauses. Lower pitch does not
guarantee more loudness, and most piezos are louder near their rated resonant
frequency (often several kHz). If still quiet, use a suitable buzzer/driver
and enclosure design. Do NOT draw more than safe ESP32 GPIO current through
the output; higher-current buzzers need a transistor driver and appropriate
power supply. Active buzzers have their own fixed pitch and may not reproduce
an externally driven 250 Hz tone faithfully.

MENU AND BUTTONS
----------------
A short (when awake): Open menu / Back / Cancel
A long:           Return to PIXO face
B short (menu):   Next item, page or setting value
B long  (menu):   Open item, preview secret, save accessory / setting
A + B together:  PEEKABOO secret; at 00:00-00:14 IST triggers MIDNIGHT GHOST
B short (face):   FEED when PIXO is asking for food
A or B from sleep: Wake PIXO; then use buttons normally

MAIN MENU
  1 My PIXO   - age, original birthday, affection, petting history, outfit,
                play-day streak, last played day, owner and favourite reaction
  2 Calendar - B hold shows BANGALORE / TODAY for 3 seconds, then Weather
                for 3 seconds, then returns to the PIXO face
  3 Battery  - placeholder, as requested
  4 Secrets  - page through seven secrets and their unlock clues
  5 Wardrobe - B tap previews; B hold equips/saves; bottom banner alternates
                between accessory name and "B:NEXT HOLD:SAVE"
  6 Weather  - B hold previews cached Bengaluru conditions for 3 seconds
  7 Settings - Brightness, Auto night, Buzzer ON/OFF, Clock sync, Owner,
                Memory, Test secrets and Factory reset

SETTINGS
--------
- Brightness: B short cycles MOON / SOFT / COZY / BRIGHT / MAX;
  B long saves to Preferences.
- Auto night: B short toggles, B long saves. The screen dims 22:00 to 07:00
  using the synced Asia/Kolkata clock, and restores normal brightness at 07:00.
- Buzzer: B short toggles ON/OFF, B long saves. No volume control exists.
- Test secrets: B short toggles ON/OFF, B long saves. When enabled,
  My PIXO > Secrets allows B long to PREVIEW the selected secret. Preview
  does not forge a birthday or award a permanent time-based milestone.
- Factory reset: Settings > Factory reset > B short to ARM, then hold B
  for 3 seconds to erase the PIXO NVS namespace and restart. THIS IS
  DESTRUCTIVE and resets birthday, growth, accessories and unlocked secrets.

WEATHER AND TIME
----------------
- Weather data: Open-Meteo's current forecast endpoint for approximate
  Bengaluru city-centre latitude 12.9716, longitude 77.5946.
- Fetch once during startup after NTP obtains the correct time, cache in
  ESP32 Preferences, and turn Wi-Fi off.
- Display: temperature in Celsius, condition, wind km/h.
- When the network fails or cache isn't for the current date, the page
  displays WEATHER UNAVAILABLE / Sync WiFi next boot.
- The first power-on with valid internet time permanently sets PIXO's
  birthday in flash memory. Normal software uploads preserve the NVS data.
- If time synchronization fails, PIXO runs but birthday/clock-based
  achievements need a successful Wi-Fi sync. No fake local date is stored.
- Weather is a single startup snapshot, NOT a continuously refreshed feed.
  Reboot to fetch again. It can become stale while PIXO remains running.
- For this prototype, the HTTPS weather request uses setInsecure() for a
  public (non-sensitive) weather endpoint. A product should validate TLS
  certificates properly.
- Open-Meteo's no-key free tier is for NON-COMMERCIAL use, subject to its
  terms and attribution requirements. Check commercial API licensing before
  using it in PIXO devices intended for sale:
  https://open-meteo.com/en/terms
  https://open-meteo.com/en/docs

PIXO FEEDING
------------
- Once every now-and-then period, PIXO says '<owner>, I'M HUNGRY!'
- When hungry, SHORT press B from the normal face to feed strawberry, juice
  or cake. The feeding animation increases affection and stores feed count.
- If ignored, PIXO goes back to sleep after the prompt period. It may wake
  occasionally to ask again later. Avoid feeding while a menu is open.
- Hungry prompt interval for regular play starts after about 15-30 min,
  subsequently 40-90 min. In test mode it is faster.
- Earlier v4 "total petting" history is preserved. Since the physical touch
  sensor was removed, feeding grows affection independently; it does NOT
  invent past touch interactions.

SECRETS (7 ENTRIES)
-------------------
1 PEEKABOO       A+B (hands cover eyes, pause, open eyes, sparkle).
2 PIXO BIRTHDAY  Original first-activation anniversary (cake+confetti).
3 ALIEN INVASION Rare chance per eligible day, beginning at age >=21 days.
                  UFO abducts one eye and returns it silly/upside-down.
4 ANGEL MODE     >=365 full calendar days since activation.
5 MIDNIGHT GHOST A+B between 00:00 and 00:14 IST (touch hardware removed).
6 ROYAL PIXO     Celebration milestones at 7, 14, 30, 90, 180, 270 days.
7 BEST FRIEND    >=365 days; a little bee becomes PIXO's companion.

All real unlock flags and milestones save in Preferences. Age is based on
calendar days since the ORIGINAL first activation, not boot count or time
powered on. First daily startup includes a pleasant morning/afternoon/
evening/night greeting using the saved owner name. The last-greeted date is
stored so it only appears once per local calendar day.

Existing blinking, happy dance, football eyes, upside-down, sleeping dream
bubbles, cat/bunny ears, wardrobe, heart balloons, birthday and the original
protected memories remain in the sketch. The TTP223 GPIO32 input/polling
and direct-touch feature have been removed.

TESTING AND KNOWN LIMITS
------------------------
The code has passed desktop C++ syntax checking against MOCK ESP32/Adafruit
interfaces for Arduino-ESP32 2.x and 3.x, and simulated flows for feeding,
menus, secrets, weather JSON extraction and birthday calculations. It has
NOT been built in a real Arduino IDE/ESP32 toolchain or tested on your exact
OLED, buttons and buzzer. Verify hardware response and font clipping.

Serial Monitor: 115200 baud.
For help, first confirm the exact ESP32 board, OLED controller and whether
the buzzer is PASSIVE or ACTIVE, as that affects actual sound output.
