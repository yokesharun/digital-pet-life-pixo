PIXO PET LIFE v3 | MENU + BRIGHTNESS + AUTO NIGHT + WARDROBE
==========================================================
Firmware for the classic ESP32 38-pin development board and a 128x64
SSD1306 OLED. Extends the prior PIXO_Pet_Life_Menu_v2 firmware.

FILES
  PIXO_Pet_Life_Menu_v3.ino  - Arduino IDE firmware
  PIXO_Pet_Life_Menu_v3.txt  - Same complete source as plain text

WHAT CHANGED IN v3
  - Five selectable OLED contrast levels with LIVE preview.
  - Settings > Brightness: B short = next level, B long = SAVE,
    A short = CANCEL / return to Settings list.
  - Settings > Auto night: ON/OFF, saved with B long.
  - Night mode dims from 22:00 to 07:00 in the configured local time
    (default IST), restoring your saved daytime level at 07:00.
  - Settings is now a dedicated submenu, not an information slideshow.
  - Main menu cards, inverse-colour headers, monochrome icons,
    larger primary text, and smaller supporting descriptions.
  - My PIXO has seven separate info pages for readable statistics.
  - Wardrobe now has 12 accessories, positioned ABOVE or BESIDE eyes.
    B short = preview next, B long = equip and save.
  - Settings > Factory reset added, with an ARM step plus 3-second
    uninterrupted B hold; cannot erase by a simple accidental press.
  - Both new display settings stored in ESP32 Preferences/NVS.
  - Existing binary PetProfile layout and touch animation timing
    preserved for compatibility with v2 saved pet data.

CONFIGURE AT TOP OF .INO
  WIFI_ENABLED       = true
  WIFI_SSID          = "YOUR_WIFI_NAME"
  WIFI_PASSWORD      = "YOUR_PASSWORD"
  PET_OWNER_NAME     = "YOUR NAME"
  BUTTON_A_PIN       = 18
  BUTTON_B_PIN       = 19
  OLED_SDA           = 25   (unchanged)
  OLED_SCL           = 26   (unchanged)
  TOUCH_PIN          = 32   (unchanged)
  TIMEZONE_GMT_OFFSET_SEC = 5*3600 + 30*60   // India / IST

  Brightness config:
  BRIGHTNESS_LEVELS  = {12, 35, 80, 145, 230}
  DEFAULT_BRIGHTNESS_LEVEL = 3     // BRIGHT; zero-based index
  DEFAULT_AUTO_NIGHT_MODE  = true  // starts ON unless previously saved
  NIGHT_CONTRAST           = 20    // dim, not a battery %
  NIGHT_START_HOUR         = 22    // 10 PM
  NIGHT_END_HOUR           = 7     // 7 AM

  SSD1306 contrast 0-255 controls OLED pixel intensity and is NOT a
  calibrated brightness percentage. Actual appearance varies by panel.
  Brightness will be temporarily higher in the Brightness editor
  to let you preview levels, even if automatic night mode is active.
  When you save/exit, night dimming will apply again until 07:00.

HARDWARE WIRING (UNCHANGED)
  OLED VCC       -> 3.3V
  OLED GND       -> GND
  OLED SDA       -> GPIO25
  OLED SCL       -> GPIO26
  TTP223 VCC     -> 3.3V
  TTP223 GND     -> GND
  TTP223 OUT     -> GPIO32
  Button A       -> GPIO18 and GND
  Button B       -> GPIO19 and GND

  For each 4-leg tactile switch: choose two terminals that are
  OPEN when released and CONNECTED only when pressed. The two pins
  on one electrically common side are NOT a valid button pair.
  Verify with a continuity tester. Switches use INPUT_PULLUP:
  no external pull-up resistors, and DO NOT connect them to 5V.

BUTTON CONTROLS
  A short     - Open main menu / go BACK one level / cancel editing
  A long      - Exit menu immediately to PIXO face (750 ms)
  B short     - Next root menu item or selected detail page
  B long      - Enter selected item or SAVE/SELECT (750 ms)
  A+B         - Hidden royal Easter egg (same as previous firmware)
  Touch       - Same petting and 1.15-sec auto-return as before

MAIN MENU
  My PIXO     - 7 info pages:
     1  age + date of birth
     2  affection level and progress bar
     3  total petting count + personality
     4  growth stage + currently worn accessory
     5  consecutive play days + Easter eggs unlocked
     6  last interaction date
     7  owner name + most frequent random reaction
  Calendar    - Japanese-style internet-synced clock/date
  Battery     - Coming soon; no battery hardware is required in v3
  Secrets     - Coming soon; A+B unlock still saved in pet profile
  Wardrobe    - 12 accessory choices, shown on real PIXO face
  Settings    - A NEW submenu (see below)

SETTINGS SUBMENU
  1 Brightness
  2 Auto night
  3 Clock sync
  4 Pet owner
  5 Pet memory
  6 Factory reset

  B short advances to next Settings entry. B long opens it.
  A short returns from detail screen to Settings list and from
  Settings list to the main menu.

SETTINGS > BRIGHTNESS
  B short  - Cycle MOON > SOFT > COZY > BRIGHT > MAX > repeat
  B long   - Save selected level to ESP32 NVS, return to Settings
  A short  - Discard uncommitted change, return to Settings
  NIGHT MODE may lower the effective contrast when 22:00-06:59.

SETTINGS > AUTO NIGHT
  B short  - Toggle ON / OFF in the editor
  B long   - Save ON/OFF to ESP32 NVS
  A short  - Cancel uncommitted change
  When ON AND the internet clock was successfully synchronized on
  this power session, PIXO dims to contrast 20 between 22:00 and
  06:59, restoring the normal saved contrast at 07:00.
  When OFF, the chosen manual contrast is used all day.
  If Wi-Fi/NTP fails on a fresh power-on, no trusted wall-clock time
  is available: PIXO keeps your normal level until NTP succeeds.
  Wi-Fi still turns OFF after startup synchronization, as before;
  ESP32 keeps the time while powered.

SETTINGS > FACTORY RESET - IMPORTANT: DELETES PET MEMORIES
  Step 1: Open Settings > Factory reset (B long).
  Step 2: B SHORT to ARM. Screen says RESET ARMED!
  Step 3: HOLD Button B continuously for 3 seconds.
  PIXO erases only its own "pixo" Preferences/NVS namespace and
  restarts. This DELETES birthday, petting count, affection, streak,
  secrets, outfit, owner stored in NVS, brightness and night settings.
  PET_OWNER_NAME in firmware is NOT erased (code configuration).
  A short cancels/back without erasing. A short also un-arms reset.
  Pet's new birthday will be set on the next valid internet sync.
  Factory reset CANNOT be undone; back up important data separately.

WARDROBE (12 ACCESSORIES)
  None, Mini bow, Cozy headphones, Mini crown, Cat ears, Bunny ears,
  Daisy, Angel halo, Star clips, Sleepy cap, Tiny horns, Heart sprout.
  All artwork is drawn in the TOP STRIP or SIDE MARGINS, outside the
  two eyes (approx X 24-58 and 70-104, Y 11-53).
  While viewing Wardrobe, B SHORT previews next item; B LONG saves
  your choice and shows SAVED. A short exits to the main menu.
  Preview changes are not saved until B is held.

PERSISTENT MEMORY / UPGRADING FROM v2
  Uses the existing ESP32 Preferences namespace "pixo".
  Existing PetProfile structure, PET_VERSION=1 and "profile" key
  are unchanged. A normal sketch upload should retain v2 birthday,
  pet count, personality, secrets, accessories and unlocked reactions.
  New keys "bright" and "night" are independent and backward-safe.
  Some IDE upload/partition/erase options DO delete NVS; select a
  normal upload, NOT "Erase All Flash".
  No SD card is required. As before, recent high-frequency pet stats
  are batch-saved about every 10s; saving is not instant for every tap.

ORIGINAL PIXO FEATURES PRESERVED
  Startup: DEVELOPED BY -> Wi-Fi time status -> PIXO logo ->
  animated eyes pushing logo -> friendly greeting -> birth/age page.
  Anniversary birthday scene, natural blinking, moving eyes/pupils,
  petting (^ ^) and hearts, random expressions, headphones/music,
  football boot kick, dizzy upside-down, peeking, happy dance,
  star pupils, blush, dreaming/sleeping, sudden wake-up,
  saved pet age and personality, A+B royal secret.

BUILD / UPLOAD
  1. Install ESP32 board support in Arduino IDE.
  2. Install Adafruit GFX Library and Adafruit SSD1306.
  3. Open PIXO_Pet_Life_Menu_v3.ino in a matching-name folder.
  4. Set Wi-Fi credentials and PET_OWNER_NAME near top.
  5. Select your CLASSIC ESP32 board and correct USB port.
  6. Upload firmware and open Serial Monitor at 115200 if needed.
  7. Test buttons and screen before assembling the enclosure.

VALIDATION
  Desktop C++ stub compile and simulated behaviour tests passed:
  original pet/menu interactions, repeated touch, pet-profile reload,
  birthday, accessories; new brightness save/cancel and NVS reload;
  day/night time logic (23:00, 03:00, 07:00, 12:00); safe reset.
  NOT yet built against your exact ESP32 Arduino installation or
  verified on actual SSD1306/TTP223 hardware. Test before production.

KNOWN TOUCH LIMITATION
  A TTP223 sensor with a slow 2-5 second HIGH release cannot detect
  separate rapid taps during that HIGH period; that is a sensor-output
  limitation, not corrected by changing menu/brightness firmware.
  The v2 default 1.15-second petting auto-return is unchanged.
