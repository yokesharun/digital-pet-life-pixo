PIXO PET LIFE v6 - CUTER REACTIONS + LONGER SECRET SHOWS
=======================================================
Version: v6 based directly on the supplied v5 Weather & Secrets firmware
Target: Classic ESP32 38-pin dev board + 0.96-inch 128x64 SSD1306 OLED

FILES
-----
PIXO_Pet_Life_v6_Cute_Animations.ino - full Arduino sketch
PIXO_Pet_Life_v6_Cute_Animations.txt - identical source as plain text
PIXO_Pet_Life_v6_README.txt - this guide

WHAT CHANGED IN V6
------------------
* User-facing PIXO messages are friendlier and more kawaii. Main menu,
  daily greeting, food request, and memory cards have been refreshed.
* Hungry after approximately 5 minutes of play (and again 5 minutes after
  a feeding or dismissed prompt). The food request dismisses itself after
  exactly 30 seconds without pressing anything.
* If PIXO was already asleep when it got hungry, it wakes to ask and, if
  ignored, settles back to sleep. The prompt is not kept on forever.
* Auto-sleep starts after 10 minutes of actual inactivity. Random animations
  do not restart this countdown. A short waking transition remains.
* Credits/Wi-Fi startup: at least 3.3 seconds plus network status hold.
  PIXO name page: 2.3 seconds. Hello/introduction: 5.5 seconds.
  Birthday/profile information: 5.2 seconds.
  Once-per-day greeting: 5.6 seconds. Birthday show: 14.6 seconds.
* Birthday and all six other secret event animations run for at least
  about 12.55 seconds (10 seconds longer than the old 2.55s secret shows).
  Each has motion across the full duration; Peekaboo repeats hand-covering
  and sparkle surprises, Alien travels and abducts/returns an eyeball, etc.
* Strawberry is now plump with leaves and seeds; juice has a straw and
  bubbles; cake has layers and decorations. Each snack has a little
  presentation, bites and a thank-you sparkle/heart reaction.
* Sleeping face now breathes gently, with curved eyelids, blush freckles,
  a cozy blanket, floating dream bubbles (fish/cake/heart), Zzz and stars.
* Secret diary has clear three-line hint text and visible status on
  separate lines with an A/B navigation footer.
* Sound remains **ON/OFF only**. Each secret uses its own ORIGINAL short
  melody; birthday is different from the other six. Feeding, hunger,
  startup, wake-up, sleeping, button presses and headphone animation also
  have custom sounds. All tunes are non-blocking and may repeat during
  longer shows. No slider and no stored volume value are added.
* Everything from v5 is kept: clock with Asia/Kolkata IST, cached Bengaluru
  weather via Open-Meteo (Wi-Fi only at startup), OLED brightness/night
  settings, factory reset protection, owner, age/birthday/NVS, two tactile
  buttons, 12 accessories, test secrets, and first greeting of the day.
* The physical touch sensor remains REMOVED. GPIO32 stays unused.

BEFORE UPLOADING
----------------
At the very top of the .ino code set:
  const char WIFI_SSID[] = "YOUR_2.4_GHZ_WIFI";
  const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
  const char PET_OWNER_NAME[] = "YOUR NAME";
Optional developer setting:
  constexpr bool TEST_MODE_DEFAULT = false;

Be aware: TEST_MODE_DEFAULT is only the default for a FRESH flash profile.
If you already toggled Test Secrets in PIXO Settings, its previously saved
NVS setting takes precedence. Disable using the menu if needed.

Arduino IDE: select your classic ESP32 board; install Adafruit GFX and
Adafruit SSD1306 libraries. ESP32 Arduino provides Wire, WiFi, Preferences,
HTTPClient, WiFiClientSecure, time and LEDC tone support.

WIRING (unchanged from v5)
-------------------------
OLED  SDA -> GPIO25
OLED  SCL -> GPIO26
OLED  VCC -> 3.3V (if your OLED module supports it)
OLED  GND -> ESP32 GND
Button A -> GPIO18 to GND, momentary tactile switch, INPUT_PULLUP
Button B -> GPIO19 to GND, momentary tactile switch, INPUT_PULLUP
PASSIVE low-current piezo buzzer + -> GPIO27, negative -> GND
Old touch module -> disconnected (GPIO32 is UNUSED)

For 4-pin tactile buttons, use one pin from each DIFFERENT electrical pair.
The two pins along the same internally joined side will not work as a
button contact. Use a continuity tester to verify leg pair selection.
Disconnect power when rewiring.

SOUND AND VOLUME
----------------
Sound ON/OFF is in Settings > Buzzer. B tap toggles, B long saves, A cancels.
The buzzer must be PASSIVE for melodies to change pitch. Frequencies used
range roughly 220-784 Hz with different rhythms. If your hardware has an
ACTIVE buzzer (built-in oscillator), it may produce a single beep or no
recognizable melody. GPIO PWM cannot guarantee loudness; many small
piezo elements are louder near their rated resonance. To get more volume,
use a suitably rated buzzer module and/or transistor driver circuit rather
than forcing high current through an ESP32 output. Never connect a buzzer
that exceeds safe GPIO current directly.

BUTTON GUIDE
------------
A short         Open menu / Back
A long          Return to PIXO face
B short         Next page / next setting; when hungry and on PIXO face, FEED
B long          Select / save current menu item
A + B together  Secret PEEKABOO; midnight 00:00-00:14 IST -> MIDNIGHT GHOST
While sleeping  Press A or B to wake; wait for wake animation to end

MAIN MENU
---------
1 My PIXO   - first activation birthday, age, affection, petting history,
              growth stage, secrets count, owner and favourite reaction
2 Calendar  - BANGALORE / TODAY for 3s -> Weather for 3s -> PIXO face
3 Battery   - placeholder; no battery percentage feature yet
4 Secrets   - use B short to browse readable hints / lock status
5 Wardrobe  - B short to preview; B long to equip and save
6 Weather   - cached Bengaluru temperature/conditions (3s preview)
7 Settings  - OLED brightness, 10 PM-7 AM night mode, Buzzer ON/OFF,
              clock sync info, owner/memory, test secrets, factory reset

HUNGER / SLEEP LOGIC
--------------------
HUNGER_INTERVAL_MS = 300000 (5 minutes)
HUNGER_PROMPT_MS   = 30000  (30 seconds)
IDLE_SLEEP_MS      = 600000 (10 minutes)

You can adjust these constants near the top of the file. Hunger displays
an owner-specific cute request on PIXO's main face. Press B briefly to
feed one of 3 rotating/random foods. The food animation runs ~4.3 seconds
and affection increases by 3 (up to 100). If not fed within 30 seconds,
PIXO stops asking without reducing affection or clearing memories.

SECRETS (ORIGINAL UNLOCK RULES KEPT)
-----------------------------------
Peekaboo: A+B anytime (except midnight ghost window).
Birthday: first-activation anniversary; celebratory cake (14.6s).
Alien Invasion: rare daily chance after age 21 days.
Angel Mode: age 365 days.
Midnight Ghost: A+B between midnight and 00:14 IST.
Royal PIXO: age 7, 14, 30, 90, 180 and 270 days.
Best Friend: age 365 days; bee friend stays on normal PIXO face.
Each non-birthday secret animation lasts 12.55 seconds.

Settings > Test secrets > ON permits previewing a selected secret by
holding B in its Secrets page, WITHOUT permanently awarding age-based
secret badges. NVS pet stats and original activation day remain unchanged.

FACTORY RESET WARNING
---------------------
Settings > Factory reset > B short ARM > B hold continuously for 3 seconds
will ERASE the PIXO memory namespace. This includes original birthday,
affection, lifetime progress, personal preferences and accessory unlocks.
The firmware never resets this data automatically. Use deliberately.

TIME / WEATHER
--------------
TZ config: Asia/Kolkata; POSIX string IST-5:30 (UTC+05:30).
Wi-Fi starts once at boot, syncs time using NTP, fetches today's Bengaluru
weather from Open-Meteo, then switches off. Weather is a startup snapshot.
Offline/failed sync shows a gentle weather-unavailable notice. The first
valid synced calendar date is saved permanently as PIXO's birthday.
Open-Meteo's free no-key service is generally for non-commercial use under
its terms. For commercial products, verify API license and attribution.
Prototype HTTPS currently does not validate the API's certificate; enable
certificate verification and review timeouts for a production device.

VERIFICATION
------------
Mock C++ integration tests were run on Arduino-ESP32 2.x and 3.x interface
simulations: button pin assignments, melody selection, settings preservation,
5-minute scheduling, 30-second hunger timeout, 10-minute inactivity sleep,
return to sleep when ignored, feeding from B button, 3s calendar/weather,
12.55-second secret exit and animation drawing. No physical ESP32 or
actual Arduino IDE library compilation is claimed. Please upload/test on
your specific display and buzzer; adjust visual clipping if needed.
Serial Monitor: 115200 baud.
