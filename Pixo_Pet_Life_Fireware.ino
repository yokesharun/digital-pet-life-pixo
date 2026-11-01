/*
  PIXO - Pet Life v4: OLED contrast, auto night, nonblocking kawaii buzzer,
  protected NVS factory reset, 12 non-overlapping cute accessories.
  Built on PIXO_Lively_WiFi_Clock.ino; OLED 128x64 classic ESP32.
  Based on PIXO_Touch_AutoReturn.ino; retains original eye drawing,
  blinking, random relaxed/bright faces, balloon hearts, wake/sleep and
  independent 1150-ms TTP223 pet auto-return.
  To set up: edit WIFI_SSID and WIFI_PASSWORD in USER SETTINGS below.

  IMPORTANT TTP223 HARDWARE LIMITATION: a slow 2-5 s HIGH signal cannot
  distinguish a short tap from a real long hold. The calendar hold gesture
  is best with USE_NATIVE_ESP32_TOUCH=1 (bare pad on GPIO32), or a TTP223
  module configured for momentary mode with fast release (not toggle).

  PIXO - Fast auto-return touch petting (ESP32 + SSD1306 128x64)
  -------------------------------------------------------------
  Default wiring: TTP223 OUT -> GPIO32, VCC -> 3V3, GND -> GND.
  OLED: SDA -> GPIO25, SCL -> GPIO26.

  FIX: Some TTP223 modules leave OUT HIGH for 2-5 seconds after a tap.
  PIXO now shows the inverted-V eyes, bounce, hearts and sparkles promptly,
  then returns to NORMAL in around 1.15 seconds from the latest physical
  digital rising edge, even if the TTP223 indicator stays lit.
  Every new OUT LOW->HIGH edge starts a fresh reaction, including edges
  received during an ongoing reaction. NO boot animation on touch.

  If you need unlimited continuous hold effects, set
  CONTINUE_PET_WHILE_TOUCH_HIGH to 1 ONLY after fixing the module's slow
  release; otherwise the 5-second stuck-HIGH problem will return.
  With bare GPIO32 capacitive electrode (USE_NATIVE_ESP32_TOUCH=1),
  true continuous hold is supported automatically until real release.

  HARDWARE LIMITATION: While TTP223 OUT stays HIGH after release, ESP32
  cannot distinguish subsequent taps; use a fast-releasing TTP223 in
  momentary mode (B/TOG jumper open), or use native ESP32 touch.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include <WiFi.h>
#include <time.h>
#include <stdio.h>
#include <Preferences.h>
// LEDC pin-vs-channel API changed in Arduino-ESP32 3.x.
#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ======================== USER SETTINGS =============================
// Wi-Fi is used ONLY during startup to set the ESP32 internal time.
// Time then continues locally while powered; Wi-Fi is switched OFF.
constexpr bool WIFI_ENABLED = true;
const char WIFI_SSID[] = "";          // <-- Enter your 2.4 GHz Wi-Fi name
const char WIFI_PASSWORD[] = "";      // <-- Enter your password
// Owner's name is stored in flash and updated whenever this setting changes.
const char PET_OWNER_NAME[] = "MY HUMAN";  // <--- EDIT OWNER NAME HERE
constexpr uint8_t BUTTON_A_PIN = 18;      // MENU / BACK, to GND
constexpr uint8_t BUTTON_B_PIN = 19;      // NEXT / SELECT, to GND
// PASSIVE (non-self-oscillating) piezo ONLY for tunable melodies/volume.
// GPIO32 is ALREADY the TTP223 touch input: DO NOT put a buzzer there!
// 2-wire passive piezo: + -> GPIO27; - -> shared GND.
// Use an NPN driver for modules drawing more current than a GPIO can safely source.
constexpr uint8_t BUZZER_PIN = 27;
constexpr bool BUZZER_ENABLED_DEFAULT = true;
constexpr uint8_t BUZZER_VOLUME_DEFAULT = 2; // 0..4; 0=mute
constexpr uint8_t BUZZER_MAX_VOLUME = 4;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 5; // Arduino-ESP32 2.x only
constexpr uint8_t BUZZER_PWM_BITS = 10;    // 0..1023 PWM duty
// Kawaii melodies are original little tone sequences (not a prerecorded song).
constexpr uint32_t BUTTON_DEBOUNCE_MS = 22;
constexpr uint32_t BUTTON_LONG_MS = 750;
constexpr uint32_t PROFILE_AUTOSAVE_MS = 10000;
// OLED SSD1306 contrast (0-255). These are NOT calibrated percentages.
// Settings > Brightness: B tap cycles; B hold SAVES; A tap cancels.
constexpr uint8_t BRIGHTNESS_LEVELS[] = {12, 35, 80, 145, 230};
const char* const BRIGHTNESS_LABELS[] = {"MOON", "SOFT", "COZY", "BRIGHT", "MAX"};
constexpr uint8_t BRIGHTNESS_LEVEL_COUNT = sizeof(BRIGHTNESS_LEVELS);
constexpr uint8_t DEFAULT_BRIGHTNESS_LEVEL = 3;
constexpr bool DEFAULT_AUTO_NIGHT_MODE = true;
constexpr uint8_t NIGHT_CONTRAST = 20;
constexpr uint8_t NIGHT_START_HOUR = 22;  // 10 PM local synced time
constexpr uint8_t NIGHT_END_HOUR = 7;     // 7 AM local synced time
constexpr uint32_t RESET_HOLD_MS = 3000; // armed reset needs uninterrupted B hold

constexpr uint32_t INTRO_HELLO_MS = 2500;
constexpr uint32_t INTRO_PROFILE_MS = 2550;
constexpr uint32_t INTRO_BIRTHDAY_MS = 4600;
constexpr uint32_t PEEK_MS = 2700;
constexpr uint32_t DANCE_MS = 2850;
constexpr uint32_t UPSIDE_DOWN_MS = 3000;
constexpr uint32_t STARRY_MS = 2350;
constexpr uint32_t SECRET_CELEBRATE_MS = 2550;
constexpr long TIMEZONE_GMT_OFFSET_SEC = 5 * 3600 + 30 * 60; // India (IST)
constexpr int TIMEZONE_DAYLIGHT_OFFSET_SEC = 0;
const char NTP_SERVER_1[] = "pool.ntp.org";
const char NTP_SERVER_2[] = "time.google.com";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 9000;
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 5500;
constexpr uint32_t WIFI_STATUS_HOLD_MS = 600;
constexpr uint32_t INTRO_FIRST_SCREEN_MIN_MS = 1750;
// Long holds require a sensor that promptly releases its output.
// With a slow-release TTP223, false calendar holds cannot be solved in code.
// Longer than the observed 2-5 s TTP223 LED tail after a single tap.
// A real long touch must last ~5.6 s on this hardware; for quick reliable
// holds switch to native GPIO32 touch (850 ms) or a fast-release sensor.
constexpr uint32_t TTP223_CALENDAR_HOLD_MS = 5600;
constexpr uint32_t NATIVE_CALENDAR_HOLD_MS = 850;
constexpr bool CALENDAR_ON_LONG_TOUCH = true;

constexpr int OLED_SDA    = 25;
constexpr int OLED_SCL    = 26;
constexpr int TOUCH_PIN   = 32;           // TTP223 OUT connects here
#ifndef USE_NATIVE_ESP32_TOUCH
#define USE_NATIVE_ESP32_TOUCH 0  // 0 = external TTP223; 1 = bare touch electrode on GPIO32 (classic ESP32 only)
#endif
constexpr uint8_t TOUCH_ACTIVE = HIGH;   // default TTP223: HIGH on touch
// If your module is configured active-LOW, change HIGH to LOW above.
constexpr uint8_t TOUCH_IDLE = (TOUCH_ACTIVE == HIGH) ? LOW : HIGH;
constexpr bool TOUCH_DEBUG = true;       // Serial Monitor 115200: TOUCH DOWN/UP

constexpr uint8_t OLED_ADDR = 0x3C;         // alternative is often 0x3D
constexpr uint32_t FRAME_MS = 33;           // about 30 FPS
constexpr uint32_t DEBOUNCE_MS = 3;          // short filter: hardware TTP223 already debounces
constexpr uint32_t HELD_HEART_EVERY_MS = 360;  // extra hearts during real held touch
constexpr uint32_t PET_AFTER_TAP_MS = 1150;   // auto-return after ~1.15 seconds
constexpr uint32_t PET_HEART_LIFETIME_MS = 750; // finish particles before hard timeout
// Default 0: ignore stuck-HIGH sensor state after PET_AFTER_TAP_MS.
// Only set to 1 if your TTP223 releases immediately when finger is lifted.
constexpr bool CONTINUE_PET_WHILE_TOUCH_HIGH = false;
// Native GPIO32 touch auto-calibrates on boot. Do not touch the electrode at boot.
constexpr uint8_t NATIVE_TOUCH_PERCENT_OF_BASELINE = 70;

constexpr bool AUTO_SLEEP = true;
constexpr uint32_t IDLE_SLEEP_MS = 60000;   // 60 seconds after last touch
constexpr uint32_t DROWSY_MS = 1800;
constexpr uint32_t WAKE_MS = 730;
constexpr uint32_t WAKE_POP_MS = 155;
constexpr uint32_t WAKE_SHAKE_MS = 500;

constexpr bool RANDOM_EXPRESSIONS = true;
constexpr uint32_t FACE_GAP_MIN_MS = 10500;
constexpr uint32_t FACE_GAP_MAX_MS = 18000;
constexpr uint32_t RELAXED_MS = 2000;
constexpr uint32_t BRIGHT_MS = 1800;

constexpr bool HEART_BALLOONS = true;
constexpr uint32_t HEART_GAP_MIN_MS = 21000; // rare, not constantly repeating
constexpr uint32_t HEART_GAP_MAX_MS = 34000;
constexpr uint32_t HEART_MS = 2850;

constexpr uint32_t INTRO_BOX_MS = 620;
constexpr uint32_t INTRO_NAME_MS = 1350;
constexpr uint32_t INTRO_CREDITS_MS = 1550;
constexpr uint32_t INTRO_OPEN_MS = 440;
constexpr uint32_t INTRO_PUSH_MS = 1430;
constexpr uint32_t WINK_MS = 1800;
constexpr uint32_t MUSIC_MS = 2900;
constexpr uint32_t EMBARRASSED_MS = 2200;
constexpr uint32_t BALL_KICK_MS = 3900;
constexpr uint32_t RANDOM_BLUSH_MS = 1250;
constexpr uint32_t BLINK_SHUT_MS = 85;
constexpr uint32_t BLINK_OPEN_MS = 130;
constexpr uint32_t DOUBLE_BLINK_PAUSE_MS = 120;

constexpr int LEFT_EYE_X = 41;
constexpr int RIGHT_EYE_X = 87;
constexpr int EYE_Y = 32;
constexpr int EYE_RX = 17;
constexpr int EYE_RY = 21;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
uint8_t previousScreen[1024];
bool hasPreviousScreen = false;

enum class State : uint8_t {
  INTRO_BOX, INTRO_NAME, INTRO_CREDITS, INTRO_PUSH, INTRO_OPEN,
  INTRO_HELLO, INTRO_PROFILE, INTRO_BIRTHDAY,
  NORMAL, RELAXED, BRIGHT, HEARTS, PET, DROWSY, SLEEP, WAKING,
  WINK, MUSIC, EMBARRASSED, BALL_KICK, CALENDAR,
  PEEK, DANCE, UPSIDE_DOWN, STARRY, MENU, SECRET_REACTION
};
enum class Blink : uint8_t { READY, CLOSING, OPENING, DOUBLE_PAUSE };
State state = State::INTRO_BOX;
Blink blink = Blink::READY;
uint32_t stateStarted = 0;
uint32_t lastInteraction = 0;
uint32_t lastFrame = 0;
uint32_t blinkStarted = 0;
uint32_t nextBlink = 0;
uint32_t nextFace = 0;
uint32_t nextHearts = 0;
uint32_t nextBlush = 0;
uint32_t blushStarted = 0;
bool blushShowing = false;

// The network never runs in the normal pet loop after the first sync.
enum class WifiStage : uint8_t { NOT_STARTED, CONNECTING, SYNCING, OK, NO_CONFIG, CONNECT_FAILED, NTP_FAILED };
WifiStage wifiStage = WifiStage::NOT_STARTED;
uint32_t wifiStageStarted = 0;
uint32_t wifiFinishedAt = 0;
bool clockSynced = false;
bool doDoubleBlink = false;

// The entire white oval moves. Pupil also moves WITHIN the oval.
float gazeX = 0.0f, gazeY = 0.0f;
float fromX = 0.0f, fromY = 0.0f;
float targetX = 0.0f, targetY = 0.0f;
float expressionGazeX = 0.0f, expressionGazeY = 0.0f;
uint32_t gazeStarted = 0, gazeTravelMs = 380, nextLook = 0;

// Debounced TTP223 output; invert TOUCH_ACTIVE for active-LOW modules.
bool lastRawTouch = TOUCH_IDLE;
bool stableTouch = TOUCH_IDLE;
uint32_t touchChangedAt = 0;
uint32_t petTapAt = 0;          // latest detected physical sensor edge
uint32_t petHoldBurstAt = 0;    // latest repeated heart burst
uint32_t touchCount = 0;
uint32_t touchDownAt = 0;
bool calendarHoldConsumed = false;
bool petAfterWake = false;
uint16_t nativeTouchThreshold = 0;

// ==================== PERSISTENT PET MEMORY ==========================
// Preferences (ESP32 NVS) survives power-off and normal firmware uploads.
// Save only on changes, and batch frequent touches to protect flash life.
constexpr uint32_t PET_MAGIC = 0x50495832UL; // "PIX2"
constexpr uint16_t PET_VERSION = 1;
enum ReactionID : uint8_t {
  R_RELAXED, R_BRIGHT, R_WINK, R_MUSIC, R_EMBARRASSED,
  R_FOOTBALL, R_PEEK, R_DANCE, R_UPSIDE, R_STARRY, R_HEARTS,
  REACTION_COUNT
};
const char* const REACTION_NAMES[REACTION_COUNT] = {
  "Relaxed", "Bright", "Wink", "Music", "Shy", "Football",
  "Peekaboo", "Dance", "Upside", "Stars", "Heart balloons"
};
struct PetProfile {
  uint32_t magic = PET_MAGIC;
  uint16_t version = PET_VERSION;
  uint16_t affection = 0;
  uint32_t birthYmd = 0;
  uint32_t totalPets = 0;
  uint32_t lastPlayedYmd = 0;
  uint32_t lastBirthdayShownYmd = 0;
  uint16_t dayStreak = 0;
  uint16_t secrets = 0;
  uint8_t personality = 0;
  uint8_t accessory = 0;
  uint8_t reserved[2] = {0,0};
  uint16_t reactions[REACTION_COUNT] = {0};
};
PetProfile pet;
Preferences petPrefs;
String savedOwner;
bool petDirty = false;
bool birthdayIsFirstActivation = false;
bool anniversaryQueued = false;
uint32_t lastProfileSavedAt = 0;
uint32_t lastValidDateCheck = 0;
const char* const PERSONALITIES[] = {"Playful","Curious","Shy","Cheerful","Dreamy"};
const char* const ACCESSORIES[] = {
  "None", "Mini bow", "Cozy headphones", "Mini crown", "Cat ears",
  "Bunny ears", "Daisy", "Angel halo", "Star clips", "Sleepy cap",
  "Tiny horns", "Heart sprout"
};
constexpr uint8_t ACCESSORY_COUNT = sizeof(ACCESSORIES)/sizeof(ACCESSORIES[0]);

// 2 momentary switches, each between an ESP32 GPIO and GND.
struct ButtonReader {
  uint8_t pin;
  bool rawDown = false;
  bool down = false;
  bool longDone = false;
  bool chordDone = false;
  uint32_t changed = 0;
  uint32_t pressedAt = 0;
};
ButtonReader buttonA{BUTTON_A_PIN};
ButtonReader buttonB{BUTTON_B_PIN};
bool bothButtonsLatched = false;
uint8_t menuIndex = 0;
uint8_t menuPage = 0;
bool menuDetail = false;
uint32_t menuFeedbackUntil = 0;
const char* const MENU_ITEMS[] = {
  "My PIXO", "Calendar", "Battery", "Secrets", "Wardrobe", "Settings"
};
constexpr uint8_t MENU_ITEMS_COUNT = sizeof(MENU_ITEMS)/sizeof(MENU_ITEMS[0]);
// Settings are separate NVS keys to preserve the v2 PetProfile binary layout.
// Do not increase PET_VERSION or change the PetProfile fields for these settings.
uint8_t savedBrightnessLevel = DEFAULT_BRIGHTNESS_LEVEL;
bool autoNightMode = DEFAULT_AUTO_NIGHT_MODE;
uint8_t editBrightnessLevel = DEFAULT_BRIGHTNESS_LEVEL;
bool editAutoNightMode = DEFAULT_AUTO_NIGHT_MODE;
// Separate NVS keys preserve all existing v2/v3 pet memory without migration.
bool buzzerEnabled = BUZZER_ENABLED_DEFAULT;
uint8_t buzzerVolume = BUZZER_VOLUME_DEFAULT;
bool editBuzzerEnabled = BUZZER_ENABLED_DEFAULT;
uint8_t editBuzzerVolume = BUZZER_VOLUME_DEFAULT;
bool contrastInitialized = false;
uint8_t lastAppliedContrast = 0;

enum class SettingsView : uint8_t { LIST, BRIGHTNESS, AUTO_NIGHT, BUZZER_ENABLE, BUZZER_VOLUME, CLOCK, OWNER, MEMORY, RESET };
SettingsView settingsView = SettingsView::LIST;
uint8_t settingsIndex = 0;
uint8_t wardrobePreview = 0; // B tap browses; B long equips and saves
bool resetArmed = false;
const char* const SETTINGS_ITEMS[] = {
  "Brightness", "Auto night", "Buzzer", "Sound level",
  "Clock sync", "Pet owner", "Pet memory", "Factory reset"
};
const char* const SETTINGS_SUBTITLES[] = {
  "OLED contrast", "22:00 to 07:00", "Cute sounds ON/OFF",
  "Volume 0 to 4", "Internet time", "Saved pet owner",
  "Saved progress", "Erase pet data"
};
constexpr uint8_t SETTINGS_COUNT = sizeof(SETTINGS_ITEMS)/sizeof(SETTINGS_ITEMS[0]);


// ======================= PIXO SOUND ENGINE ===========================
// Nonblocking: no delay() or tone() calls in the animation loop.
// Each note is (frequency in Hz, duration in milliseconds). 0 Hz is a rest.
// Uses LEDC PWM; duty changes the strength of a passive piezo's fundamental
// tone, but this is NOT a calibrated acoustic dB/percentage volume control.
struct PixoNote { uint16_t hz; uint16_t ms; };

// Original little PIXO greeting: bouncy arpeggios plus a falling sparkle.
const PixoNote STARTUP_TUNE[] = {
  {659,105},{0,32},{784,125},{988,140},{0,45},
  {1175,180},{988,105},{1319,260},{0,65},{1568,200},
  {1319,140},{1175,240}
};
const PixoNote WAKE_TUNE[] = {
  {784,75},{1047,80},{1319,95},{1568,155}
};
const PixoNote SLEEP_TUNE[] = {
  {659,115},{587,155},{494,210},{392,280},{0,120}
};
// Occasional VERY soft sleeping purr. Only one short sound every 25-45 s.
const PixoNote DREAM_TUNE[] = {{392,85},{0,50},{330,105}};
const PixoNote BUTTON_A_TUNE[] = {{784,44},{587,52}};
const PixoNote BUTTON_B_TUNE[] = {{988,52},{1319,68}};
const PixoNote PET_TUNE[] = {{1319,58},{1568,78}};
const PixoNote SECRET_TUNE[] = {{784,90},{988,95},{1175,95},{1568,240}};
const PixoNote BIRTHDAY_TUNE[] = {
  {784,115},{988,115},{1175,150},{0,80},
  {1047,125},{1319,150},{1568,310}
};
// Original playful tune for the headphones animation, ~2.6 seconds.
const PixoNote MUSIC_TUNE[] = {
  {523,145},{659,145},{784,145},{1047,180},{0,70},
  {988,155},{784,145},{659,145},{784,190},{0,65},
  {587,140},{784,140},{988,180},{1175,260}
};
const PixoNote VOLUME_PREVIEW_TUNE[] = {{1047,105},{1319,150}};

enum class SoundID : uint8_t {
  NONE, STARTUP, WAKE, SLEEP, DREAM, BUTTON_A, BUTTON_B, PET,
  MUSIC, SECRET, BIRTHDAY, PREVIEW
};
SoundID activeSound = SoundID::NONE;
const PixoNote* activeNotes = nullptr;
uint8_t activeNoteCount = 0;
uint8_t activeNoteIndex = 0;
uint32_t activeNoteBegan = 0;
uint32_t nextDreamChirp = 0;
bool buzzerReady = false;
bool forceSoundPreview = false;
// Volume steps are intentionally gentle. A passive piezo's loudness is not
// linear in PWM duty; actual results vary by transducer and enclosure.
const uint16_t SOUND_DUTY[] = {0, 22, 65, 145, 325};

void buzzerPwmSet(uint16_t frequency, uint8_t volume) {
  if(!buzzerReady) return;
  if(frequency==0 || volume==0) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(BUZZER_PIN,0);
#else
    ledcWrite(BUZZER_LEDC_CHANNEL,0);
#endif
    return;
  }
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_PIN,frequency);
  ledcWrite(BUZZER_PIN,SOUND_DUTY[volume]);
#else
  ledcWriteTone(BUZZER_LEDC_CHANNEL,frequency);
  ledcWrite(BUZZER_LEDC_CHANNEL,SOUND_DUTY[volume]);
#endif
}
void buzzerHalt() {
  buzzerPwmSet(0,0);
  activeSound=SoundID::NONE;
  activeNotes=nullptr;
  activeNoteCount=0;
  forceSoundPreview=false;
}
void buzzerInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  buzzerReady=ledcAttach(BUZZER_PIN,1600,BUZZER_PWM_BITS);
#else
  ledcSetup(BUZZER_LEDC_CHANNEL,1600,BUZZER_PWM_BITS);
  ledcAttachPin(BUZZER_PIN,BUZZER_LEDC_CHANNEL);
  buzzerReady=true;
#endif
  buzzerPwmSet(0,0);
  if(!buzzerReady) Serial.println(F("PIXO: buzzer PWM setup failed"));
}
void buzzerPlay(SoundID id,uint32_t now,bool preview=false) {
  if(!buzzerReady || ((!buzzerEnabled || buzzerVolume==0) && !preview)) return;
  if(preview && (!editBuzzerEnabled || editBuzzerVolume==0)) {
    buzzerHalt();return;
  }
  switch(id) {
    case SoundID::STARTUP: activeNotes=STARTUP_TUNE;activeNoteCount=sizeof(STARTUP_TUNE)/sizeof(PixoNote);break;
    case SoundID::WAKE: activeNotes=WAKE_TUNE;activeNoteCount=sizeof(WAKE_TUNE)/sizeof(PixoNote);break;
    case SoundID::SLEEP: activeNotes=SLEEP_TUNE;activeNoteCount=sizeof(SLEEP_TUNE)/sizeof(PixoNote);break;
    case SoundID::DREAM: activeNotes=DREAM_TUNE;activeNoteCount=sizeof(DREAM_TUNE)/sizeof(PixoNote);break;
    case SoundID::BUTTON_A: activeNotes=BUTTON_A_TUNE;activeNoteCount=sizeof(BUTTON_A_TUNE)/sizeof(PixoNote);break;
    case SoundID::BUTTON_B: activeNotes=BUTTON_B_TUNE;activeNoteCount=sizeof(BUTTON_B_TUNE)/sizeof(PixoNote);break;
    case SoundID::PET: activeNotes=PET_TUNE;activeNoteCount=sizeof(PET_TUNE)/sizeof(PixoNote);break;
    case SoundID::MUSIC: activeNotes=MUSIC_TUNE;activeNoteCount=sizeof(MUSIC_TUNE)/sizeof(PixoNote);break;
    case SoundID::SECRET: activeNotes=SECRET_TUNE;activeNoteCount=sizeof(SECRET_TUNE)/sizeof(PixoNote);break;
    case SoundID::BIRTHDAY: activeNotes=BIRTHDAY_TUNE;activeNoteCount=sizeof(BIRTHDAY_TUNE)/sizeof(PixoNote);break;
    case SoundID::PREVIEW: activeNotes=VOLUME_PREVIEW_TUNE;activeNoteCount=sizeof(VOLUME_PREVIEW_TUNE)/sizeof(PixoNote);break;
    default: buzzerHalt();return;
  }
  activeSound=id;
  activeNoteIndex=0;
  activeNoteBegan=now;
  forceSoundPreview=preview;
  buzzerPwmSet(activeNotes[0].hz,id==SoundID::DREAM ? 1 :
               (preview?editBuzzerVolume:buzzerVolume));
}
void buzzerUpdate(uint32_t now) {
  if(activeSound==SoundID::NONE) return;
  if(!forceSoundPreview && (!buzzerEnabled || buzzerVolume==0)) {
    buzzerHalt();return;
  }
  if(forceSoundPreview && (!editBuzzerEnabled || editBuzzerVolume==0)) {
    buzzerHalt();return;
  }
  // Advance with bounded work; keeps eye animations, Wi-Fi and buttons live.
  if(now-activeNoteBegan>=activeNotes[activeNoteIndex].ms) {
    ++activeNoteIndex;
    if(activeNoteIndex>=activeNoteCount) {buzzerHalt();return;}
    activeNoteBegan=now;
    buzzerPwmSet(activeNotes[activeNoteIndex].hz,activeSound==SoundID::DREAM ? 1 :
                 (forceSoundPreview?editBuzzerVolume:buzzerVolume));
  }
}

bool inSettingsDetail() {return state==State::MENU && menuDetail && menuIndex==5;}
// Saved contrast always returns after exiting preview; night mode only works
// if a valid internet time was received during this power session.
bool isNightTime() {
  if(!autoNightMode || !clockSynced) return false;
  const time_t t=time(nullptr);
  if(t < (time_t)1700000000) return false;
  tm local;
  if(!localtime_r(&t,&local)) return false;
  return local.tm_hour>=NIGHT_START_HOUR || local.tm_hour<NIGHT_END_HOUR;
}
void updateDisplayContrast() {
  uint8_t wanted = BRIGHTNESS_LEVELS[savedBrightnessLevel];
  if (inSettingsDetail() && settingsView==SettingsView::BRIGHTNESS)
    wanted = BRIGHTNESS_LEVELS[editBrightnessLevel]; // editor preview overrides night
  else if(isNightTime() && wanted>NIGHT_CONTRAST) wanted=NIGHT_CONTRAST;
  if(!contrastInitialized || wanted!=lastAppliedContrast) {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(wanted);
    lastAppliedContrast=wanted;
    contrastInitialized=true;
  }
}
void loadDisplayPreferences() {
  savedBrightnessLevel=petPrefs.getUChar("bright",DEFAULT_BRIGHTNESS_LEVEL);
  if(savedBrightnessLevel>=BRIGHTNESS_LEVEL_COUNT) savedBrightnessLevel=DEFAULT_BRIGHTNESS_LEVEL;
  autoNightMode=petPrefs.getBool("night",DEFAULT_AUTO_NIGHT_MODE);
  editBrightnessLevel=savedBrightnessLevel;
  editAutoNightMode=autoNightMode;
  buzzerEnabled=petPrefs.getBool("sndon",BUZZER_ENABLED_DEFAULT);
  buzzerVolume=petPrefs.getUChar("sndvol",BUZZER_VOLUME_DEFAULT);
  if(buzzerVolume>BUZZER_MAX_VOLUME) buzzerVolume=BUZZER_VOLUME_DEFAULT;
  editBuzzerEnabled=buzzerEnabled;
  editBuzzerVolume=buzzerVolume;
}


// Gregorian civil-day ordinal: days since Unix epoch in a fixed local date.
int32_t civilDay(int32_t y, uint32_t m, uint32_t d) {
  y -= (m<=2);
  int32_t era=(y>=0 ? y : y-399)/400;
  uint32_t yoe=(uint32_t)(y-era*400);
  int32_t mm=(int32_t)m;
  uint32_t doy=(153*(uint32_t)(mm+(mm>2 ? -3 : 9))+2)/5+d-1;
  uint32_t yday=yoe*365+yoe/4-yoe/100+doy;
  return era*146097+(int32_t)yday-719468;
}
int32_t dayOrdinal(uint32_t ymd) {
  if (!ymd) return -1;
  return civilDay((int32_t)(ymd/10000), (ymd/100)%100, ymd%100);
}
uint32_t currentLocalDate() {
  time_t raw=time(nullptr);
  if (raw < (time_t)1700000000) return 0;
  tm value;
  if (!localtime_r(&raw,&value)) return 0;
  return (uint32_t)(value.tm_year+1900)*10000UL +
         (uint32_t)(value.tm_mon+1)*100UL + (uint32_t)value.tm_mday;
}
uint32_t petAgeDays() {
  uint32_t today=clockSynced ? currentLocalDate() : 0;
  if (!today || !pet.birthYmd) return 0;
  int32_t d=dayOrdinal(today)-dayOrdinal(pet.birthYmd);
  return d>0 ? (uint32_t)d : 0;
}
const char* growthStage() {
  if (!pet.birthYmd || !clockSynced) return "Unknown";
  uint32_t d=petAgeDays();
  return d<2 ? "Baby" : d<8 ? "Toddler" : d<31 ? "Playful" :
         d<91 ? "Teen" : "Grown-up";
}
void markPetDirty() { petDirty=true; }
void savePetIfNeeded(uint32_t now, bool force=false) {
  if (!petDirty) return;
  if (!force && now-lastProfileSavedAt < PROFILE_AUTOSAVE_MS) return;
  petPrefs.putBytes("profile", &pet, sizeof(pet));
  petDirty=false;
  lastProfileSavedAt=now;
}
void loadDisplayPreferences();
void loadPetMemory() {
  petPrefs.begin("pixo", false);
  if (petPrefs.getBytesLength("profile")==sizeof(pet))
    petPrefs.getBytes("profile", &pet, sizeof(pet));
  if (pet.magic!=PET_MAGIC || pet.version!=PET_VERSION) {
    pet=PetProfile();
    pet.personality=(uint8_t)random(5);
    markPetDirty();
  }
  if (pet.accessory>=ACCESSORY_COUNT) {
    pet.accessory=0; markPetDirty();
  }
  savedOwner=String(PET_OWNER_NAME);
  savedOwner.trim();
  if (savedOwner.length()==0) savedOwner="MY HUMAN";
  if (savedOwner.length()>20) savedOwner.remove(20);
  if (petPrefs.getString("owner", "")!=savedOwner)
    petPrefs.putString("owner",savedOwner);
  savePetIfNeeded(millis(),true);
  loadDisplayPreferences();
}
void notePlayedToday() {
  if (!clockSynced) return;
  uint32_t today=currentLocalDate();
  if (!today || today==pet.lastPlayedYmd) return;
  if (pet.lastPlayedYmd &&
      dayOrdinal(today)-dayOrdinal(pet.lastPlayedYmd)==1)
    pet.dayStreak = pet.dayStreak<65535 ? pet.dayStreak+1 : 65535;
  else pet.dayStreak=1;
  pet.lastPlayedYmd=today;
  markPetDirty();
}
uint8_t unlockedCount() {
  uint16_t bits=pet.secrets;
  uint8_t n=0;
  while(bits) { n+=(bits&1); bits>>=1; }
  return n;
}
const char* favouriteReaction() {
  uint16_t best=0;
  uint8_t idx=0;
  for (uint8_t i=0;i<REACTION_COUNT;i++)
    if (pet.reactions[i]>best) {best=pet.reactions[i];idx=i;}
  return best ? REACTION_NAMES[idx] : "Not yet";
}
void recordReaction(uint8_t reaction) {
  if (reaction>=REACTION_COUNT) return;
  if (pet.reactions[reaction]<65535) ++pet.reactions[reaction];
  markPetDirty();
}
void recordPetTouch() {
  if (pet.totalPets<UINT32_MAX) ++pet.totalPets;
  uint32_t petScore=pet.totalPets/2;
  pet.affection=(uint16_t)(petScore>100 ? 100 : petScore);
  markPetDirty();
  notePlayedToday();
}
// An anniversary is celebrated once per calendar year on the activation
// month/day. Feb 29 birthdays celebrate on Feb 28 in non-leap years.
bool isAnniversary(uint32_t today) {
  if (!today || !pet.birthYmd || today<=pet.birthYmd) return false;
  if (today%10000 == pet.birthYmd%10000) return true;
  if (pet.birthYmd%10000 == 229 && today%10000 == 228) {
    int y=(int)(today/10000);
    bool leap=(y%4==0 && (y%100!=0 || y%400==0));
    return !leap;
  }
  return false;
}
void onClockReady() {
  uint32_t today=currentLocalDate();
  if (!today) return;
  if (pet.birthYmd==0) {
    pet.birthYmd=today;
    birthdayIsFirstActivation=true;
    markPetDirty();
    notePlayedToday();
    savePetIfNeeded(millis(),true); // never lose first activation date
    Serial.print(F("PIXO born on ")); Serial.println(today);
  }
  if (isAnniversary(today) && pet.lastBirthdayShownYmd!=today) {
    anniversaryQueued=true;
    // Record before animation so a reboot won't repeat it endlessly.
    pet.lastBirthdayShownYmd=today;
    pet.secrets |= (1U<<1); // hidden birthday badge
    markPetDirty();
    savePetIfNeeded(millis(),true);
  }
}


void checkPetDateDuringRuntime(uint32_t now) {
  // Catch a birthday at midnight even when PIXO has stayed powered on.
  if(!clockSynced || now-lastValidDateCheck < 60000) return;
  lastValidDateCheck=now;
  uint32_t today=currentLocalDate();
  if(isAnniversary(today) && pet.lastBirthdayShownYmd!=today) {
    anniversaryQueued=true;
    pet.lastBirthdayShownYmd=today;
    pet.secrets |= (1U<<1);
    markPetDirty();
    savePetIfNeeded(now,true);
  }
}

struct HeartBurst {
  uint32_t began = 0;
  bool valid = false;
};
constexpr uint8_t MAX_PET_HEART_BURSTS = 6;
HeartBurst petBursts[MAX_PET_HEART_BURSTS];
uint8_t nextBurstSlot = 0;
void spawnPetHearts(uint32_t now) {
  petBursts[nextBurstSlot].began = now;
  petBursts[nextBurstSlot].valid = true;
  nextBurstSlot = (nextBurstSlot + 1) % MAX_PET_HEART_BURSTS;
  petHoldBurstAt = now;
}

// ======================== DRAWING HELPERS ============================
float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
float ease(float x) { x = clamp01(x); return x*x*(3.0f - 2.0f*x); }
float progress(uint32_t dt, uint32_t duration) {
  return clamp01((float)dt / (float)duration);
}
int mixInt(int a, int b, float t) {
  return (int)roundf(a + (b-a)*ease(t));
}
bool reached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}
void oval(int cx, int cy, int rx, int ry, uint16_t color) {
  if (rx <= 0 || ry <= 0) return;
  for (int y = -ry; y <= ry; ++y) {
    const float k = 1.0f - (float)(y*y)/(float)(ry*ry);
    int w = (int)roundf(rx * sqrtf(k > 0 ? k : 0));
    display.drawFastHLine(cx-w, cy+y, 2*w+1, color);
  }
}
void oneEye(int x, int y, float openness, int pupilShift, int extra = 0) {
  const int hh = (int)roundf((EYE_RY + extra) * clamp01(openness));
  if (hh < 2) {
    display.drawFastHLine(x-13, y, 27, SSD1306_WHITE);
    return;
  }
  oval(x, y, EYE_RX + extra, hh, SSD1306_WHITE);
  if (hh >= 9) {
    int px = x + constrain(pupilShift, -5, 5);
    oval(px, y+1, 7, (hh-3 < 10 ? hh-3 : 10), SSD1306_BLACK);
    if (hh >= 15) display.fillCircle(px-2, y-3, 2, SSD1306_WHITE);
  }
}
void eyes(float openness, float look = 0, float dx = 0, float dy = 0,
          float surprise = 0) {
  int shift = (int)roundf(look * 1.55f + dx);
  int y = EYE_Y + (int)roundf(dy);
  int pupil = (int)roundf(look * 0.52f);
  int extra = (int)roundf(clamp01(surprise) * 2.0f);
  oneEye(LEFT_EYE_X + shift, y, openness, pupil, extra);
  oneEye(RIGHT_EYE_X + shift, y, openness, pupil, extra);
}
void thickLine(int x1, int y1, int x2, int y2, int thickness = 1) {
  for (int d=-thickness; d<=thickness; ++d)
    display.drawLine(x1, y1+d, x2, y2+d, SSD1306_WHITE);
  display.fillCircle(x1, y1, thickness, SSD1306_WHITE);
  display.fillCircle(x2, y2, thickness, SSD1306_WHITE);
}

// Only petting may call this inverted-V facial expression.
void happyPetEyes(float amount, float lift=0.0f, float sway=0.0f) {
  const float t = ease(amount);
  const int up = (int)roundf(lift);
  const int sideMove = (int)roundf(sway);
  for (int i=0; i<2; ++i) {
    // Both ^ eyes move smoothly LEFT-RIGHT and UP-DOWN together.
    const int cx = (i ? RIGHT_EYE_X : LEFT_EYE_X) + sideMove;
    const int top = mixInt(32, 18, t) - up;
    const int sides = mixInt(32, 35, t) - up;
    const int width = mixInt(10, 15, t);
    thickLine(cx-width, sides, cx, top, 2);
    thickLine(cx, top, cx+width, sides, 2);
  }
}

// The tiny floating hearts are a TOUCH-ONLY effect, not a random expression.
void tinyHeart(int cx, int cy, int radius) {
  if (radius < 2) return;
  display.fillCircle(cx-radius/2, cy-radius/3, radius/2, SSD1306_WHITE);
  display.fillCircle(cx+radius/2, cy-radius/3, radius/2, SSD1306_WHITE);
  display.fillTriangle(cx-radius, cy, cx+radius, cy,
                       cx, cy+radius, SSD1306_WHITE);
}
void tinySparkle(int x, int y, int r) {
  if (r < 1) return;
  display.drawFastVLine(x, y-r, r*2+1, SSD1306_WHITE);
  display.drawFastHLine(x-r, y, r*2+1, SSD1306_WHITE);
}
void drawPetCuteFx(uint32_t now) {
  // Independent particle bursts: a rapid 2nd/3rd tap adds more hearts
  // rather than erasing hearts from the previous tap.
  for (uint8_t i = 0; i < MAX_PET_HEART_BURSTS; ++i) {
    if (!petBursts[i].valid) continue;
    const uint32_t age = now - petBursts[i].began;
    if (age > PET_HEART_LIFETIME_MS) {
      petBursts[i].valid = false;
      continue;
    }
    const float rise = ease(progress(age, PET_HEART_LIFETIME_MS));
    const float fadeIn = ease(progress(age, 90));
    const float fadeOut = 1.0f - ease(progress(age > 510 ? age - 510 : 0, 240));
    const int radius = (int)roundf(5.4f * fadeIn * fadeOut);
    const float wobble = sinf((float)age * 0.012f);
    const int y = mixInt(42, 5, rise);
    tinyHeart(mixInt(18, 10, rise) + (int)roundf(wobble * 2), y, radius);
    tinyHeart(mixInt(110, 118, rise) - (int)roundf(wobble * 2), y, radius);

    if (age < 340) {
      const float sparkle = ease(progress(age, 100)) *
          (1.0f - ease(progress(age > 165 ? age - 165 : 0, 175)));
      tinySparkle(64, 12, (int)roundf(3.0f * sparkle));
    }
  }
}

// Heart shape using a smooth parametric outline filled with triangles.
// scale 1.0 is approx 32x29 px, appropriate for the two eye locations.
void heart(int cx, int cy, float scale) {
  if (scale < 0.05f || cy < -30) return;
  const int steps = 24;
  const float tau = 6.283185307f;
  int previousX = 0, previousY = 0;
  for (int i=0; i<=steps; ++i) {
    float a = tau * ((float)i / steps);
    float sa = sinf(a);
    float hx = 16.0f * sa*sa*sa;
    float hy = -(13.0f*cosf(a) - 5.0f*cosf(2*a)
               - 2.0f*cosf(3*a) - cosf(4*a));
    int x = cx + (int)roundf(hx * scale);
    int y = cy + (int)roundf(hy * scale);
    if (i) display.fillTriangle(cx, cy+2, previousX, previousY,
                                x, y, SSD1306_WHITE);
    previousX=x; previousY=y;
  }
}

// Transition oval eyes -> two hearts -> balloon-style upward flight -> eyes.
void drawHeartBalloons(uint32_t dt) {
  if (dt < 175) {
    // Start with a soft blink so the change of shape feels deliberate.
    float t = ease(progress(dt, 175));
    eyes(1.0f-t, expressionGazeX*(1.0f-t), 0,
         expressionGazeY*(1.0f-t));
    return;
  }
  if (dt < 2300) {
    float size;
    float fly = 0.0f;
    if (dt < 460) {
      float pop = ease(progress(dt-175, 285));
      size = 0.15f + 0.85f*pop + 0.13f*sinf(pop*3.14159265f);
    } else {
      size = 1.0f + 0.045f*sinf((float)(dt-460)*0.0070f);
    }
    if (dt >= 720) fly = ease(progress(dt-720, 1580));

    // Float fully off-screen (including the thin balloon strings).
    float height = 92.0f*fly;
    float sway = 3.4f*sinf((float)dt*0.0085f)*fly;
    int leftX = (int)roundf(LEFT_EYE_X + sway - 3.0f*fly);
    int rightX = (int)roundf(RIGHT_EYE_X - sway + 3.0f*fly);
    int cy = (int)roundf(EYE_Y - height);

    heart(leftX, cy, size);
    heart(rightX, cy, size);

    // Balloon strings are not facial details, and vanish as hearts fly away.
    if (fly > 0.05f && cy > -28) {
      int startY = cy + (int)roundf(15.0f*size);
      display.drawLine(leftX, startY, leftX-2, startY+6, SSD1306_WHITE);
      display.drawLine(leftX-2, startY+6, leftX, startY+11, SSD1306_WHITE);
      display.drawLine(rightX, startY, rightX+2, startY+6, SSD1306_WHITE);
      display.drawLine(rightX+2, startY+6, rightX, startY+11, SSD1306_WHITE);
    }
    return;
  }
  // Return to the usual two eyes with a gentle opening animation.
  eyes(ease(progress(dt-2300, 380)));
}


// ==================== EXTRA EXPRESSIONS ==============================
// These expressions are new states and never replace the original
// relaxed/bright/heart/blink animations. All use frame-based drawing.
void drawBlush(int leftX = 17, int rightX = 111, int y = 46) {
  for (int side=0; side<2; ++side) {
    const int x=side ? rightX : leftX;
    for (int k=-1;k<=1;++k) {
      display.drawLine(x+k*4-2,y+2,x+k*4+1,y-2,SSD1306_WHITE);
    }
  }
}
void drawWink(uint32_t dt) {
  float phase=ease(progress(dt,290));
  if (dt>WINK_MS-340) phase=1.0f-ease(progress(dt-(WINK_MS-340),340));
  int bob=(int)roundf(1.7f*sinf((float)dt*.009f));
  oneEye(LEFT_EYE_X,EYE_Y+bob,0.85f,-2);
  // Right eyelid becomes a tilted, cheeky curved line.
  if (phase>.14f) {
    const int y=EYE_Y+bob;
    thickLine(RIGHT_EYE_X-12,y+2,RIGHT_EYE_X-2,y-2,1);
    thickLine(RIGHT_EYE_X-2,y-2,RIGHT_EYE_X+13,y+3,1);
    tinySparkle(114,15+(int)(2*sinf(dt*.012f)),3);
  } else {
    oneEye(RIGHT_EYE_X,EYE_Y+bob,1.0f,2);
  }
  if (phase>.3f) drawBlush();
}
void musicNote(int x,int y,bool doubleNote=false) {
  display.fillCircle(x,y,3,SSD1306_WHITE);
  display.drawFastVLine(x+3,y-13,12,SSD1306_WHITE);
  display.drawFastHLine(x+3,y-13,7,SSD1306_WHITE);
  if (doubleNote) {
    display.fillCircle(x+12,y+2,3,SSD1306_WHITE);
    display.drawFastVLine(x+15,y-11,12,SSD1306_WHITE);
    display.drawLine(x+3,y-13,x+15,y-11,SSD1306_WHITE);
  }
}
void drawMusic(uint32_t dt) {
  float wave=sinf((float)dt*.014f);
  int shake=(int)roundf(wave*3);
  int bob=(int)roundf(fabsf(sinf((float)dt*.014f))*2.7f);
  eyes(0.82f,0,shake,-bob);
  // A little headphone band arched above both eyes.
  int previousX=0, previousY=0;
  for (int i=0;i<=24;i++) {
    float t=(float)i/24.0f;
    int x=12+shake+(int)roundf(104*t);
    int y=29-bob-(int)roundf(25*sinf(3.14159265f*t));
    if (i) thickLine(previousX,previousY,x,y,1);
    previousX=x; previousY=y;
  }
  display.fillRoundRect(7+shake,25-bob,10,20,4,SSD1306_WHITE);
  display.fillRoundRect(111+shake,25-bob,10,20,4,SSD1306_WHITE);
  display.drawRoundRect(10+shake,28-bob,5,14,2,SSD1306_BLACK);
  display.drawRoundRect(113+shake,28-bob,5,14,2,SSD1306_BLACK);
  int drift=(int)roundf(3.0f*sinf((float)dt*.005f));
  musicNote(20+drift,14+(int)(3*sinf(dt*.009f)));
  musicNote(105-drift,10+(int)(2*cosf(dt*.011f)),true);
  if ((dt/170)%3 == 0) tinySparkle(64,56,2);
}
void drawEmbarrassed(uint32_t dt) {
  int sh=(int)roundf(3*sinf((float)dt*.006f));
  // Eyes peek away and gently scrunch with embarrassment.
  float o=0.72f+0.08f*sinf((float)dt*.011f);
  oneEye(LEFT_EYE_X-2,EYE_Y+sh,o,4);
  oneEye(RIGHT_EYE_X-2,EYE_Y+sh,o,4);
  drawBlush(18,110,46);
  // No mouth in PIXO expressions. Animated sweat drop, drawn at edge of the face.
  int drop=12+(int)(5*ease(progress(dt,1600)));
  display.fillTriangle(112,drop,108,drop+7,116,drop+7,SSD1306_WHITE);
  display.fillCircle(112,drop+7,3,SSD1306_WHITE);
  if ((dt/180)%2==0) tinySparkle(20,11,2);
}
// ================= FOOTBALL EYES + PLAYER KICK =====================
// Each eye turns into a football with a rotating pentagonal panel pattern.
void soccerBall(int cx, int cy, int radius, float roll) {
  if (cx+radius<0 || cx-radius>=128 || cy+radius<0 || cy-radius>=64) return;
  display.fillCircle(cx,cy,radius,SSD1306_WHITE);
  // Pentagonal central black panel and five connected seam panels.
  int vx[5],vy[5];
  for (int i=0;i<5;++i) {
    float a=roll-1.5707963f+i*1.25663706f;
    vx[i]=cx+(int)roundf(cosf(a)*5.8f);
    vy[i]=cy+(int)roundf(sinf(a)*5.8f);
  }
  for (int i=1;i<4;++i)
    display.fillTriangle(vx[0],vy[0],vx[i],vy[i],vx[i+1],vy[i+1],SSD1306_BLACK);
  for (int i=0;i<5;++i) {
    float a=roll-1.5707963f+i*1.25663706f;
    int ex=cx+(int)roundf(cosf(a)*(radius-2));
    int ey=cy+(int)roundf(sinf(a)*(radius-2));
    display.drawLine(vx[i],vy[i],ex,ey,SSD1306_BLACK);
    // Short white stitching on the dark central patch.
    if (i==0) display.drawPixel(cx,cy,SSD1306_WHITE);
  }
  display.drawCircle(cx,cy,radius,SSD1306_WHITE);
}
// One footballer's boot + shin + knee, swinging from the left of the display.
void drawFootballerLeg(float kick) {
  const float k=ease(kick);
  const int hipX=-5, hipY=56;
  const int kneeX=mixInt(25,36,k), kneeY=mixInt(47,32,k);
  const int ankleX=mixInt(40,54,k), ankleY=mixInt(56,31,k);
  // Shorts, bent thigh and rising calf give the silhouette a football pose.
  display.fillRect(0,48,12,15,SSD1306_WHITE);
  display.fillTriangle(0,48,16,48,8,59,SSD1306_WHITE);
  for(int w=-4;w<=4;++w) {
    display.drawLine(hipX+w,hipY,kneeX+w/2,kneeY,SSD1306_WHITE);
    display.drawLine(kneeX+w/2,kneeY,ankleX+w/2,ankleY,SSD1306_WHITE);
  }
  // Sock bands and ankle outline.
  display.drawLine(kneeX-3,kneeY+8,kneeX+5,kneeY+9,SSD1306_BLACK);
  display.drawLine(kneeX-2,kneeY+11,kneeX+6,kneeY+12,SSD1306_BLACK);
  // Distinct football boot: heel, toe, laces, black sole and two cleats.
  display.fillRoundRect(ankleX-7,ankleY-5,25,11,4,SSD1306_WHITE);
  display.fillTriangle(ankleX+10,ankleY-5,ankleX+24,ankleY+3,
                       ankleX+10,ankleY+5,SSD1306_WHITE);
  display.drawLine(ankleX-6,ankleY+5,ankleX+22,ankleY+5,SSD1306_BLACK);
  for(int i=0;i<3;++i)
    display.drawLine(ankleX+1+i*4,ankleY-3,ankleX+3+i*4,ankleY,SSD1306_BLACK);
  display.drawFastVLine(ankleX+2,ankleY+6,3,SSD1306_WHITE);
  display.drawFastVLine(ankleX+17,ankleY+6,3,SSD1306_WHITE);
}
void drawBallKick(uint32_t dt) {
  // 0-550ms: eyes turn into footballs and bounce in anticipation.
  // 550-1180ms: real kicking-leg silhouette strikes the left football.
  // Both footballs tumble offscreen with acceleration, then return.
  if (dt < 550) {
    int hop=(int)roundf(8*fabsf(sinf(dt*.014f)));
    soccerBall(LEFT_EYE_X,32-hop,14,dt*.008f);
    soccerBall(RIGHT_EYE_X,32-hop,14,-dt*.008f);
    if(dt>270) drawFootballerLeg(0.2f*progress(dt-270,280));
  } else if (dt<1830) {
    const float t=progress(dt-550,1280);
    const float fly=ease(progress(dt-720,1030));
    int lx=LEFT_EYE_X+(int)roundf(151*fly);
    int rx=RIGHT_EYE_X+(int)roundf(141*ease(progress(dt-860,970)));
    int ly=32-(int)roundf(12*sinf(t*9.5f));
    int ry=32+(int)roundf(10*sinf(t*11.5f));
    soccerBall(lx,ly,14,dt*.030f);
    soccerBall(rx,ry,14,-dt*.035f);
    if (dt<1260) drawFootballerLeg(progress(dt-550,570));
    if(dt>650 && dt<1350) tinySparkle(75,34,3);
  } else if (dt<2300) {
    display.setTextSize(2); display.setCursor(33,20);display.print(F("GOAL!"));
    if((dt/160)%2==0) {tinySparkle(18,14,4); tinySparkle(110,51,4);}
  } else {
    float t=ease(progress(dt-2300,BALL_KICK_MS-2300));
    int travel=(int)roundf(152*(1-t));
    int bounce=(int)roundf(9*fabsf(sinf(t*11))*(1-t));
    soccerBall(LEFT_EYE_X+travel,32-bounce,14,dt*.021f);
    soccerBall(RIGHT_EYE_X+travel,32+bounce/2,14,-dt*.025f);
    if(dt>BALL_KICK_MS-380) drawBlush(15,113,49);
  }
}

// ==================== NEW LIVELY EXPRESSIONS ==========================
void drawBouncyEye(int cx,int cy,int rx,int ry,int pupilX=0) {
  oval(cx,cy,rx,ry,SSD1306_WHITE);
  oval(cx+pupilX,cy+1,6,8,SSD1306_BLACK);
  display.fillCircle(cx+pupilX-2,cy-4,2,SSD1306_WHITE);
}
void drawCuriousPeek(uint32_t dt) {
  // One eye peers in from the left; the second follows from the right.
  float a=ease(progress(dt,630));
  float b=ease(progress(dt>510?dt-510:0,750));
  int left=mixInt(-20,LEFT_EYE_X,a);
  int right=mixInt(148,RIGHT_EYE_X,b);
  int hop=(dt>1300 && dt<2050) ? (int)roundf(3*sinf(dt*.016f)) : 0;
  oneEye(left,EYE_Y+hop,0.92f,4);
  oneEye(right,EYE_Y-hop,0.92f,-4);
  if (dt>1100 && dt<2200) tinySparkle(64,9,3);
  if (dt>1900) drawBlush(18,110,48);
}
void drawHappyDance(uint32_t dt) {
  // Left/right hopping, horizontal squash, vertical stretch: 100% frame based.
  float beat=(float)dt*.012f;
  float hop=fabsf(sinf(beat));
  float side=7*sinf(beat*.66f);
  float squash=0.5f+0.5f*sinf(beat*2.0f);
  int rx=(int)roundf(19-5*squash);
  int ry=(int)roundf(17+5*squash);
  int leftX=LEFT_EYE_X+(int)roundf(side);
  int rightX=RIGHT_EYE_X+(int)roundf(side);
  int y=EYE_Y-(int)roundf(9*hop);
  drawBouncyEye(leftX,y,rx,ry,-2);
  drawBouncyEye(rightX,y,rx,ry,2);
  if((dt/220)%2==0) {tinySparkle(15,16,2); tinySparkle(114,47,2);}
  if(dt>1350) drawBlush(15,113,49);
}
void drawUpsideDown(uint32_t dt) {
  // Rotate the pair around the centre of the face, pause upside-down,
  // panic/shake, then spin them back. Pupils/glints invert with the eyes.
  float angle=0;
  if(dt<850) angle=3.14159265f*ease(progress(dt,850));
  else if(dt<1640) angle=3.14159265f;
  else angle=3.14159265f*(1.0f-ease(progress(dt-1640,1050)));
  float panic=(dt>900 && dt<1850) ? 2.4f*sinf(dt*.060f) : 0;
  int dx=(int)roundf(23*cosf(angle));
  int dy=(int)roundf(15*sinf(angle));
  int lx=64-dx+(int)roundf(panic);
  int rx=64+dx+(int)roundf(panic);
  int ly=EYE_Y-dy;
  int ry=EYE_Y+dy;
  // Eye glints appear underneath during the upside-down part.
  float invert=(1.0f-cosf(angle))*0.5f;
  for (int i=0;i<2;i++) {
    int x=(i?rx:lx), y=(i?ry:ly);
    oval(x,y,14,17,SSD1306_WHITE);
    oval(x+(i?2:-2),y,6,8,SSD1306_BLACK);
    display.fillCircle(x-2,y+(int)roundf(-5+10*invert),2,SSD1306_WHITE);
  }
  if(dt>880 && dt<2100) {
    tinySparkle(13,12,3); tinySparkle(113,51,3);
    if((dt/160)%2==0) display.drawLine(60,3,65,8,SSD1306_WHITE);
  }
}
void drawStarShape(int cx,int cy,int outer) {
  int px[10],py[10];
  for(int i=0;i<10;i++) {
    float a=-1.5707963f+i*3.14159265f/5;
    int radius=(i%2) ? outer/2 : outer;
    px[i]=cx+(int)roundf(cosf(a)*radius);
    py[i]=cy+(int)roundf(sinf(a)*radius);
  }
  for(int i=1;i<9;i++)
    display.fillTriangle(px[0],py[0],px[i],py[i],px[i+1],py[i+1],SSD1306_WHITE);
}
void drawStarry(uint32_t dt) {
  int bob=(int)roundf(6*fabsf(sinf(dt*.012f)));
  for(int i=0;i<2;++i) {
    int x=i?RIGHT_EYE_X:LEFT_EYE_X;
    oval(x,EYE_Y-bob,17,20,SSD1306_WHITE);
    display.fillCircle(x,EYE_Y-bob,10,SSD1306_BLACK);
    drawStarShape(x,EYE_Y-bob,8);
  }
  if((dt/180)%2==0) {
    tinySparkle(13,12,3);tinySparkle(112,17,3);
    tinySparkle(64,55,2);
  }
}
// All accessory artwork lives in the outer margins of the 128x64 face:
// eye ovals occupy approximately X=24..58 / 70..104 and Y=11..53.
// Do NOT draw opaque shapes over these rectangles. No eye occlusion.
void drawAccessoryOverlay(int accessory) {
  switch(accessory) {
    case 1: { // Mini ribbon bow: tiny top-center hair clip
      display.fillTriangle(54,2,62,6,54,10,SSD1306_WHITE);
      display.fillTriangle(74,2,66,6,74,10,SSD1306_WHITE);
      display.fillCircle(64,6,3,SSD1306_WHITE);
      display.fillCircle(64,6,1,SSD1306_BLACK);
      break;
    }
    case 2: { // Headphones: headband in TOP edge, pads in SIDE margins
      for(int x=23;x<=105;x+=2) {
        int y=1+(int)roundf(7.0f*(1.0f-powf(fabsf(x-64)/41.0f,1.6f)));
        // curve is intentionally above the eyes: 1..8 pixels
        display.drawPixel(x,y,SSD1306_WHITE);
        display.drawPixel(x,y+1,SSD1306_WHITE);
      }
      display.fillRoundRect(13,22,8,19,3,SSD1306_WHITE);
      display.fillRoundRect(107,22,8,19,3,SSD1306_WHITE);
      display.drawFastVLine(16,26,11,SSD1306_BLACK);
      display.drawFastVLine(110,26,11,SSD1306_BLACK);
      break;
    }
    case 3: { // Crown sits in top CENTER gap, not on eye surfaces
      display.fillTriangle(53,8,55,0,59,8,SSD1306_WHITE);
      display.fillTriangle(60,8,64,0,68,8,SSD1306_WHITE);
      display.fillTriangle(69,8,73,0,75,8,SSD1306_WHITE);
      display.fillRoundRect(53,7,22,3,1,SSD1306_WHITE);
      display.drawPixel(64,5,SSD1306_BLACK);
      break;
    }
    case 4: { // Cat ears, above the left and right eye
      display.fillTriangle(28,9,34,0,44,9,SSD1306_WHITE);
      display.fillTriangle(84,9,94,0,100,9,SSD1306_WHITE);
      display.drawLine(34,4,33,7,SSD1306_BLACK);
      display.drawLine(94,4,95,7,SSD1306_BLACK);
      break;
    }
    case 5: { // Bunny ears, clipped above display top
      display.fillRoundRect(35,0,7,10,3,SSD1306_WHITE);
      display.fillRoundRect(86,0,7,10,3,SSD1306_WHITE);
      display.drawFastVLine(38,2,5,SSD1306_BLACK);
      display.drawFastVLine(89,2,5,SSD1306_BLACK);
      break;
    }
    case 6: { // Daisy, upper-center between eyes
      for(int a=0;a<6;++a) {
        float th=a*1.04719755f;
        display.fillCircle(64+(int)roundf(5*cosf(th)),5+(int)roundf(3*sinf(th)),2,SSD1306_WHITE);
      }
      display.fillCircle(64,5,3,SSD1306_BLACK);
      display.drawPixel(64,5,SSD1306_WHITE);
      break;
    }
    case 7: { // Angel halo in upper-center gap
      display.drawRoundRect(53,2,22,7,4,SSD1306_WHITE);
      display.drawRoundRect(56,4,16,3,2,SSD1306_WHITE);
      tinySparkle(14,9,2);tinySparkle(114,9,2);
      break;
    }
    case 8: { // Star clips outside the sides of the eyes
      tinySparkle(13,25,6);tinySparkle(115,25,6);
      tinySparkle(10,40,2);tinySparkle(118,40,2);
      break;
    }
    case 9: { // Sleep cap tucked into upper-left margin; no eye overlay
      display.fillTriangle(17,9,27,0,45,9,SSD1306_WHITE);
      display.drawFastHLine(18,9,27,SSD1306_WHITE);
      display.fillCircle(16,6,3,SSD1306_WHITE);
      display.drawLine(27,5,32,5,SSD1306_BLACK);
      break;
    }
    case 10: { // Tiny horns, two pointed tabs along top border
      display.fillTriangle(34,9,36,1,44,9,SSD1306_WHITE);
      display.fillTriangle(84,9,92,1,94,9,SSD1306_WHITE);
      display.drawPixel(38,7,SSD1306_BLACK);
      display.drawPixel(90,7,SSD1306_BLACK);
      break;
    }
    case 11: { // Heart antenna: middle gap between eyes
      display.drawFastVLine(64,8,6,SSD1306_WHITE);
      tinyHeart(64,5,4);
      break;
    }
    default:break;
  }
}

void drawBirthday(uint32_t dt) {
  // Two surprised eyes + cake with candle + confetti, without a mouth.
  float hop=fabsf(sinf(dt*.009f))*3;
  oneEye(34,36-(int)hop,.80f,-1);
  oneEye(94,36-(int)hop,.80f,1);
  display.drawRoundRect(52,39,24,13,3,SSD1306_WHITE);
  display.fillRect(54,45,20,5,SSD1306_WHITE);
  display.drawFastHLine(51,39,26,SSD1306_WHITE);
  display.drawFastVLine(64,32,7,SSD1306_WHITE);
  display.fillCircle(64,29+(int)(2*sinf(dt*.019f)),2,SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(21,2);display.print(F("HAPPY BIRTHDAY!"));
  if((dt/220)%2==0) {tinySparkle(14,23,3); tinySparkle(112,19,3);}
  for(int i=0;i<9;i++) {
    int x=(i*41+11)%122+3;
    int y=((int)(dt/13)+(i*23))%63;
    display.drawPixel(x,y,SSD1306_WHITE);
  }
  display.setCursor(43,55);display.print(F("PIXO  <3"));
}
void drawSecret(uint32_t dt) {
  drawStarry(dt);
  display.fillTriangle(49,10,53,1,59,10,SSD1306_WHITE);
  display.fillTriangle(58,10,64,0,70,10,SSD1306_WHITE);
  display.fillTriangle(69,10,75,1,79,10,SSD1306_WHITE);
  display.fillRect(49,10,30,4,SSD1306_WHITE);
  display.fillRect(18,54,92,9,SSD1306_BLACK);
  display.setTextSize(1);display.setCursor(20,55);
  display.print(F("SECRET FOUND!"));
}
void drawIntroHello(uint32_t dt) {
  display.setTextSize(1);
  display.setCursor(24,4);display.print(F("Hi, I'm your"));
  display.setCursor(12,17);display.print(F("digital pet PIXO!"));
  int bounce=(int)roundf(3*fabsf(sinf(dt*.009f)));
  oneEye(LEFT_EYE_X,45-bounce,0.75f,-2);
  oneEye(RIGHT_EYE_X,45-bounce,0.75f,2);
  if(dt>650) tinyHeart(64,45,5);
}
void fmtDate(char* out,size_t n,uint32_t ymd) {
  if(!ymd) {snprintf(out,n,"Not synced");return;}
  snprintf(out,n,"%04lu-%02lu-%02lu",(unsigned long)(ymd/10000),
           (unsigned long)((ymd/100)%100),(unsigned long)(ymd%100));
}
void drawIntroProfile(uint32_t dt) {
  display.drawRoundRect(2,1,124,62,5,SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(8,6);display.print(F("PIXO  /  MY DAY"));
  display.drawFastHLine(10,17,108,SSD1306_WHITE);
  char d[18];fmtDate(d,sizeof(d),pet.birthYmd);
  display.setCursor(11,24);display.print(F("Born: "));display.print(d);
  display.setCursor(11,36);display.print(F("Age: "));
  if(clockSynced && pet.birthYmd) {
    display.print((unsigned long)petAgeDays());display.print(F(" days"));
  } else display.print(F("Waiting for time"));
  display.setCursor(11,49);display.print(F("Owner: "));
  display.print(savedOwner.substring(0,12));
  if((dt/200)%2==0) tinySparkle(113,49,2);
}

// ================= JAPANESE STATIONERY CALENDAR =====================
// Adafruit's built-in font has no Japanese kanji/kana. This intentionally
// uses a Japanese-inspired minimal calendar layout and sakura decorations.
void drawSakura(int x,int y,int r) {
  for (int i=0;i<5;i++) {
    float a=(6.2831853f*i/5.0f)-1.5707963f;
    display.fillCircle(x+(int)roundf(cosf(a)*r),
                       y+(int)roundf(sinf(a)*r),2,SSD1306_WHITE);
  }
  display.fillCircle(x,y,2,SSD1306_BLACK);
}
void drawCalendar(uint32_t now) {
  const uint32_t elapsed=now-stateStarted;
  int slide=(int)roundf((1.0f-ease(progress(elapsed,230)))*18);
  display.drawRoundRect(1,1,126,62,6,SSD1306_WHITE);
  display.drawFastHLine(8,15,112,SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(9,5);display.print(F("PIXO"));
  display.setCursor(67,5);display.print(F("KYO / TODAY"));
  time_t raw=time(nullptr);
  if (!clockSynced || raw < (time_t)1700000000) {
    display.setCursor(13,23);display.print(F("TIME NOT SYNCED"));
    display.setCursor(13,39);display.print(F("SET WIFI ABOVE"));
    drawSakura(110,38,5);
    return;
  }
  struct tm t;
  localtime_r(&raw,&t);
  static const char* DAYS[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  static const char* MONTHS[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  char dateBuf[40];
  char clockBuf[10];
  snprintf(dateBuf,sizeof(dateBuf),"%04d.%02d.%02d",1900+t.tm_year,1+t.tm_mon,t.tm_mday);
  snprintf(clockBuf,sizeof(clockBuf),"%02d:%02d",t.tm_hour,t.tm_min);
  display.setCursor(34,18+slide); display.print(dateBuf);
  display.setTextSize(2);
  display.setCursor(30,32+slide);display.print(clockBuf);
  display.setTextSize(1);
  char secBuf[4]; snprintf(secBuf,sizeof(secBuf),"%02d",t.tm_sec);
  display.setCursor(97,41+slide);display.print(secBuf);
  display.setCursor(39,55);display.print(DAYS[t.tm_wday]);
  display.setCursor(65,55);display.print(MONTHS[t.tm_mon]);
  drawSakura(14,39+(int)(2*sinf(now*.003f)),5);
  drawSakura(113,25+(int)(2*cosf(now*.004f)),3);
  if ((now/700)%2==0) tinySparkle(117,52,2);
}

// ====================== POLISHED 128x64 OLED MENU =====================
// Layout hierarchy: large selected item, tiny context/description,
// inverse-colour header, consistently sized footer and small pictograms.
void setState(State next,uint32_t now);
void scheduleExpressions(uint32_t now);

void uiIcon(uint8_t kind,int x,int y,uint16_t ink) {
  // Hand-drawn 12x12 monochrome glyphs; no extra libraries or bitmap assets.
  switch(kind%9) {
    case 0: // face
      display.drawRoundRect(x,y,12,12,3,ink);
      display.fillCircle(x+3,y+5,1,ink);display.fillCircle(x+9,y+5,1,ink);
      break;
    case 1: // calendar
      display.drawRect(x+1,y+2,10,9,ink);display.drawFastHLine(x+1,y+5,10,ink);
      display.drawFastVLine(x+4,y,4,ink);display.drawFastVLine(x+9,y,4,ink);
      break;
    case 2: // battery
      display.drawRoundRect(x,y+3,10,7,1,ink);
      display.fillRect(x+10,y+5,2,3,ink);
      break;
    case 3: // gift
      display.drawRect(x+1,y+5,10,7,ink);display.drawFastHLine(x,y+4,12,ink);
      display.drawFastVLine(x+6,y+4,8,ink);display.drawCircle(x+4,y+2,2,ink);
      display.drawCircle(x+8,y+2,2,ink);
      break;
    case 4: // hanger
      display.drawCircle(x+6,y+2,2,ink);
      display.drawLine(x+6,y+4,x+1,y+9,ink);display.drawLine(x+1,y+9,x+11,y+9,ink);
      display.drawLine(x+11,y+9,x+6,y+4,ink);
      break;
    case 5: // gear
      display.drawCircle(x+6,y+6,5,ink);display.drawCircle(x+6,y+6,2,ink);
      for(int k=0;k<4;k++) {display.drawPixel(x+6+(k==0?6:k==1?-6:0),y+6+(k==2?6:k==3?-6:0),ink);}
      break;
    case 6: // sun/brightness
      display.drawCircle(x+6,y+6,3,ink);
      display.drawFastHLine(x,y+6,2,ink);display.drawFastHLine(x+10,y+6,2,ink);
      display.drawFastVLine(x+6,y,2,ink);display.drawFastVLine(x+6,y+10,2,ink);
      break;
    case 7: // moon
      display.fillCircle(x+6,y+6,5,ink);
      display.fillCircle(x+9,y+3,4,ink==SSD1306_WHITE?SSD1306_BLACK:SSD1306_WHITE);
      break;
    default: // lock/erase
      display.drawRoundRect(x+2,y+5,8,7,1,ink);
      display.drawRoundRect(x+4,y+1,4,7,2,ink);
      break;
  }
}
void uiHeader(const char* title,uint8_t icon,const char* right="") {
  display.fillRect(0,0,128,14,SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  uiIcon(icon,3,1,SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(19,3);display.print(title);
  if(right[0]) {display.setCursor(104,3);display.print(right);}
  display.setTextColor(SSD1306_WHITE);
}
void uiFooter(const char* left,const char* middle="",const char* right="") {
  display.drawFastHLine(0,54,128,SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(2,56);display.print(left);
  if(middle[0]) {display.setCursor(52,56);display.print(middle);}
  if(right[0]) {display.setCursor(92,56);display.print(right);}
}
void uiLabel(int x,int y,const char* label) {
  display.setTextSize(1);
  display.setCursor(x,y);display.print(label);
}
void uiValue(int x,int y,const String& value) {
  display.setTextSize(2);
  display.setCursor(x,y);display.print(value.substring(0,10));
  display.setTextSize(1);
}
void uiMiniBar(int x,int y,int w,uint8_t value,uint8_t maximum) {
  display.drawRoundRect(x,y,w,7,2,SSD1306_WHITE);
  int fill=maximum ? (w-4)*value/maximum : 0;
  if(fill>0)display.fillRect(x+2,y+2,fill,3,SSD1306_WHITE);
}
void uiCardItem(const char* title,const char* desc,uint8_t glyph,uint8_t idx,uint8_t count) {
  char serial[8];snprintf(serial,sizeof(serial),"%02u/%02u",(unsigned)(idx+1),(unsigned)count);
  uiHeader("PIXO / MENU",0,serial);
  display.drawRoundRect(4,19,25,24,5,SSD1306_WHITE);
  uiIcon(glyph,10,25,SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(31,22);display.print(title);
  display.setTextSize(1);
  display.setCursor(6,45);display.print(desc);
  uiFooter("A:BACK","B:NEXT","H:OPEN");
}
void drawMyPixoDetail() {
  char page[6];snprintf(page,sizeof(page),"%u/7",(unsigned)(menuPage+1));
  uint8_t cardIcon = menuPage==0?0:menuPage==1?6:menuPage==2?0:
                     menuPage==3?4:menuPage==4?3:menuPage==5?1:0;
  uiHeader("MY PIXO",cardIcon,page);
  char tmp[30];
  if(menuPage==0) {
    uiLabel(5,17,"AGE  /  LITTLE COMPANION");
    if(clockSynced && pet.birthYmd) {
      snprintf(tmp,sizeof(tmp),"%lu days",(unsigned long)petAgeDays());
      uiValue(5,28,tmp);
    } else uiValue(5,28,"No clock");
    char dob[18];fmtDate(dob,sizeof(dob),pet.birthYmd);
    uiLabel(5,45,"BORN");uiLabel(36,45,dob);
  } else if(menuPage==1) {
    uiLabel(5,17,"LOVE  /  AFFECTION");
    snprintf(tmp,sizeof(tmp),"%u / 100",(unsigned)pet.affection);
    uiValue(5,28,tmp);
    uiMiniBar(5,46,113,(uint8_t)pet.affection,100);
    // Total pets available on next card as a large number.
  } else if(menuPage==2) {
    uiLabel(5,17,"TOTAL PETTING COUNT");
    snprintf(tmp,sizeof(tmp),"%lu",(unsigned long)pet.totalPets);uiValue(5,28,tmp);
    uiLabel(5,45,"MOOD");uiLabel(43,45,PERSONALITIES[pet.personality%5]);
  } else if(menuPage==3) {
    uiLabel(5,17,"GROWTH / OUTFIT");
    uiValue(5,28,growthStage());
    uiLabel(5,45,"WEAR:");uiLabel(42,45,ACCESSORIES[pet.accessory]);
  } else if(menuPage==4) {
    uiLabel(5,17,"DAY STREAK / SECRETS");
    snprintf(tmp,sizeof(tmp),"%u days",(unsigned)pet.dayStreak);uiValue(5,28,tmp);
    snprintf(tmp,sizeof(tmp),"UNLOCKED EASTER EGGS: %u",(unsigned)unlockedCount());
    uiLabel(5,45,tmp);
  } else if(menuPage==5) {
    uiLabel(5,17,"LAST INTERACTION");
    char last[18];fmtDate(last,sizeof(last),pet.lastPlayedYmd);
    uiValue(5,28,last);
    uiLabel(5,45,"Last day you played");
  } else {
    uiLabel(5,17,"PET OWNER");
    uiValue(5,28,savedOwner);
    uiLabel(5,45,"FAV:");uiLabel(32,45,favouriteReaction());
  }
  uiFooter("A:BACK","B:NEXT",page);
}
void drawSettingsList() {
  uiHeader("SETTINGS",5,"EDIT");
  const char* name=SETTINGS_ITEMS[settingsIndex];
  uiIcon(settingsIndex==0?6:settingsIndex==1?7:settingsIndex==5?8:5,8,21,SSD1306_WHITE);
  if(strlen(name)<=8) uiValue(25,20,name);
  else {
    // Long labels remain readable instead of being cut off in size-2 font.
    char num[5];snprintf(num,sizeof(num),"%02u",(unsigned)(settingsIndex+1));
    uiValue(25,20,num);
    uiLabel(50,25,name);
  }
  uiLabel(8,45,SETTINGS_SUBTITLES[settingsIndex]);
  uiFooter("A:BACK","B:NEXT","H:OPEN");
}
void drawSettingsPage(uint32_t now) {
  if(settingsView==SettingsView::LIST) {drawSettingsList();return;}
  if(settingsView==SettingsView::BRIGHTNESS) {
    uiHeader("BRIGHTNESS",6,"OLED");
    uiLabel(6,17,"CONTRAST  /  PREVIEW");
    uiValue(6,28,BRIGHTNESS_LABELS[editBrightnessLevel]);
    uiMiniBar(6,46,115,editBrightnessLevel+1,BRIGHTNESS_LEVEL_COUNT);
    uiFooter("A:BACK","B:NEXT","H:SAVE");
  } else if(settingsView==SettingsView::AUTO_NIGHT) {
    uiHeader("AUTO NIGHT",7,"22-7");
    uiLabel(6,17,"DIM AFTER 10 PM");
    uiValue(8,28,editAutoNightMode ? "ON" : "OFF");
    uiLabel(6,45,clockSynced?"Uses internet clock":"Needs time sync");
    uiFooter("A:BACK","B:TOGG","H:SAVE");
  } else if(settingsView==SettingsView::BUZZER_ENABLE) {
    uiHeader("PIXO SOUNDS",5,"AUDIO");
    uiLabel(6,17,"CUTE CHIRPS & MUSIC");
    uiValue(8,29,editBuzzerEnabled ? "ON" : "OFF");
    uiLabel(6,46,"Tap B to toggle");
    uiFooter("A:BACK","B:TOGG","H:SAVE");
  } else if(settingsView==SettingsView::BUZZER_VOLUME) {
    uiHeader("SOUND LEVEL",5,"0-4");
    uiLabel(6,17,editBuzzerEnabled?"PIEZO PWM VOLUME":"SOUND CURRENTLY OFF");
    char levelLabel[17];
    snprintf(levelLabel,sizeof(levelLabel),"LEVEL %u",(unsigned)editBuzzerVolume);
    uiValue(6,28,levelLabel);
    uiMiniBar(6,46,115,editBuzzerVolume,BUZZER_MAX_VOLUME);
    uiFooter("A:BACK","B:TEST","H:SAVE");
  } else if(settingsView==SettingsView::CLOCK) {
    uiHeader("CLOCK SYNC",1);
    uiLabel(7,19,clockSynced?"INTERNET TIME READY":"TIME NOT SYNCED");
    uiLabel(7,32,"Wi-Fi is OFF now");
    uiLabel(7,45,"India time (IST)");
    uiFooter("A:BACK");
  } else if(settingsView==SettingsView::OWNER) {
    uiHeader("PET OWNER",0);
    uiLabel(7,18,"MY FAVOURITE HUMAN");
    uiValue(7,28,savedOwner);
    uiLabel(7,48,"Change in firmware");
    uiFooter("A:BACK");
  } else if(settingsView==SettingsView::MEMORY) {
    uiHeader("PET MEMORY",3);
    uiLabel(7,20,"SAVED IN ESP32 FLASH");
    uiLabel(7,34,"No SD card needed");
    uiLabel(7,45,"Keeps birth & pets");
    uiFooter("A:BACK");
  } else if(settingsView==SettingsView::RESET) {
    uiHeader("FACTORY RESET",8);
    if(!resetArmed) {
      uiLabel(7,19,"ERASE ALL PET MEMORY?");
      uiLabel(7,32,"Birthday, pets, gifts");
      uiLabel(7,45,"B tap to ARM reset");
      uiFooter("A:BACK","B:ARM");
    } else {
      uiLabel(7,17,"RESET ARMED!  !!");
      uiLabel(7,30,"HOLD B FOR 3 SEC");
      int fill=0;
      if(buttonB.down) {
        uint32_t held=now-buttonB.pressedAt;
        if(held>RESET_HOLD_MS)held=RESET_HOLD_MS;
        fill=(int)(held*114UL/RESET_HOLD_MS);
      }
      display.drawRect(6,43,116,8,SSD1306_WHITE);
      if(fill>0)display.fillRect(7,44,fill,6,SSD1306_WHITE);
      uiFooter("A:CANCEL","B:DISARM");
    }
  }
}
void drawWardrobe(uint32_t now) {
  // Authentic face preview: accessories occupy ONLY top/side margins.
  eyes(.88f,0.0f,0,0);
  drawAccessoryOverlay(wardrobePreview);
  display.fillRect(0,53,128,11,SSD1306_BLACK);
  display.drawFastHLine(0,53,128,SSD1306_WHITE);
  display.setTextSize(1);display.setCursor(2,56);
  display.print(ACCESSORIES[wardrobePreview]);
  char item[7];snprintf(item,sizeof(item),"%u/%u",(unsigned)(wardrobePreview+1),(unsigned)ACCESSORY_COUNT);
  display.setCursor(99,56);display.print(item);
  if(!reached(now,menuFeedbackUntil)) {
    display.fillRect(47,0,48,10,SSD1306_BLACK);
    display.setCursor(50,1);display.print(F("SAVED"));
  }
}
void drawMenu(uint32_t now) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  if(!menuDetail) {
    static const char* const subtitles[]={
      "Your pet's memories", "Time and calendar", "Coming soon",
      "Hidden little wonders", "Cute outfits for PIXO", "Make PIXO your own"
    };
    uiCardItem(MENU_ITEMS[menuIndex],subtitles[menuIndex],menuIndex,menuIndex,MENU_ITEMS_COUNT);
    return;
  }
  if(menuIndex==0) drawMyPixoDetail();
  else if(menuIndex==1) drawCalendar(now);
  else if(menuIndex==2) {
    uiHeader("BATTERY",2);
    uiIcon(2,54,20,SSD1306_WHITE);
    uiLabel(27,42,"Coming soon");uiFooter("A:BACK");
  } else if(menuIndex==3) {
    uiHeader("SECRETS",3);
    uiIcon(3,54,20,SSD1306_WHITE);
    uiLabel(24,42,"Coming soon");uiFooter("A:BACK");
  } else if(menuIndex==4) drawWardrobe(now);
  else if(menuIndex==5) drawSettingsPage(now);
  display.setTextColor(SSD1306_WHITE);
}
void showMenu(uint32_t now) {
  menuDetail=false;menuIndex=0;menuPage=0;
  settingsView=SettingsView::LIST;settingsIndex=0;resetArmed=false;
  setState(State::MENU,now);
  lastInteraction=now;
}
void menuNext(uint32_t now) {
  lastInteraction=now;
  if(!menuDetail) {menuIndex=(menuIndex+1)%MENU_ITEMS_COUNT;return;}
  if(menuIndex==0) menuPage=(menuPage+1)%7;
  else if(menuIndex==4) wardrobePreview=(wardrobePreview+1)%ACCESSORY_COUNT;
  else if(menuIndex==5) {
    switch(settingsView) {
      case SettingsView::LIST: settingsIndex=(settingsIndex+1)%SETTINGS_COUNT;break;
      case SettingsView::BRIGHTNESS: editBrightnessLevel=(editBrightnessLevel+1)%BRIGHTNESS_LEVEL_COUNT;break;
      case SettingsView::AUTO_NIGHT: editAutoNightMode=!editAutoNightMode;break;
      case SettingsView::BUZZER_ENABLE:
        editBuzzerEnabled=!editBuzzerEnabled;
        // Preview when toggling ON; no sound at all when toggled OFF.
        if(editBuzzerEnabled) buzzerPlay(SoundID::PREVIEW,now,true);
        else buzzerHalt();
        break;
      case SettingsView::BUZZER_VOLUME:
        editBuzzerVolume=(editBuzzerVolume+1)%(BUZZER_MAX_VOLUME+1);
        buzzerPlay(SoundID::PREVIEW,now,true);
        break;
      case SettingsView::RESET: resetArmed=!resetArmed;break;
      default:break;
    }
  }
}
void menuSelect(uint32_t now) {
  lastInteraction=now;
  if(!menuDetail) {
    menuDetail=true;menuPage=0;settingsView=SettingsView::LIST;
    if(menuIndex==4)wardrobePreview=pet.accessory;
  } else if(menuIndex==4) {
    pet.accessory=wardrobePreview;
    markPetDirty();savePetIfNeeded(now,true);
    menuFeedbackUntil=now+650;
  } else if(menuIndex==5) {
    if(settingsView==SettingsView::LIST) {
      settingsView=(SettingsView)(settingsIndex+1);
      if(settingsView==SettingsView::BRIGHTNESS)editBrightnessLevel=savedBrightnessLevel;
      if(settingsView==SettingsView::AUTO_NIGHT)editAutoNightMode=autoNightMode;
      if(settingsView==SettingsView::BUZZER_ENABLE)editBuzzerEnabled=buzzerEnabled;
      if(settingsView==SettingsView::BUZZER_VOLUME)editBuzzerVolume=buzzerVolume;
      if(settingsView==SettingsView::RESET)resetArmed=false;
    } else if(settingsView==SettingsView::BRIGHTNESS) {
      savedBrightnessLevel=editBrightnessLevel;
      petPrefs.putUChar("bright",savedBrightnessLevel);
      settingsView=SettingsView::LIST;
      Serial.println(F("PIXO brightness saved"));
    } else if(settingsView==SettingsView::AUTO_NIGHT) {
      autoNightMode=editAutoNightMode;
      petPrefs.putBool("night",autoNightMode);
      settingsView=SettingsView::LIST;
      Serial.println(F("PIXO automatic night saved"));
    } else if(settingsView==SettingsView::BUZZER_ENABLE) {
      buzzerEnabled=editBuzzerEnabled;
      petPrefs.putBool("sndon",buzzerEnabled);
      settingsView=SettingsView::LIST;
      if(!buzzerEnabled) buzzerHalt();
      Serial.println(F("PIXO buzzer ON/OFF saved"));
    } else if(settingsView==SettingsView::BUZZER_VOLUME) {
      buzzerVolume=editBuzzerVolume;
      petPrefs.putUChar("sndvol",buzzerVolume);
      settingsView=SettingsView::LIST;
      buzzerHalt();
      Serial.println(F("PIXO buzzer volume saved"));
    }
  }
}
void menuBack(uint32_t now) {
  lastInteraction=now;
  if(state!=State::MENU) {showMenu(now);return;}
  if(!menuDetail) {setState(State::NORMAL,now);scheduleExpressions(now);return;}
  if(menuIndex==5 && settingsView!=SettingsView::LIST) {
    // Cancel changes without writing them to flash.
    settingsView=SettingsView::LIST;
    editBrightnessLevel=savedBrightnessLevel;
    editAutoNightMode=autoNightMode;
    editBuzzerEnabled=buzzerEnabled;
    editBuzzerVolume=buzzerVolume;
    if(forceSoundPreview) buzzerHalt();
    resetArmed=false;
    return;
  }
  menuDetail=false;menuPage=0;
}
void menuHome(uint32_t now) {
  lastInteraction=now;
  setState(State::NORMAL,now);
  scheduleExpressions(now);
  savePetIfNeeded(now,true);
  resetArmed=false;
  settingsView=SettingsView::LIST;
}
void factoryResetPet() {
  // Deliberate confirmation: Settings > Factory reset > B tap ARM > B hold 3 s.
  // Erases this pet's NVS namespace ONLY (not the whole ESP32 flash).
  // A fresh first-activation date will be set on the next successful NTP sync.
  petPrefs.clear();
  Serial.println(F("PIXO factory reset: pet memories and display preferences erased"));
  delay(80);
  ESP.restart();
}
bool isIntroState(State st) {
  return st==State::INTRO_CREDITS || st==State::INTRO_BOX ||
    st==State::INTRO_NAME || st==State::INTRO_PUSH ||
    st==State::INTRO_OPEN || st==State::INTRO_HELLO ||
    st==State::INTRO_PROFILE || st==State::INTRO_BIRTHDAY;
}
void buttonShort(ButtonReader &b,uint32_t now) {
  if(isIntroState(state)) return;
  notePlayedToday();
  const bool soundEditor=inSettingsDetail() &&
    (settingsView==SettingsView::BUZZER_ENABLE || settingsView==SettingsView::BUZZER_VOLUME);
  if(b.pin==BUTTON_A_PIN) menuBack(now);
  else if(state==State::MENU) menuNext(now);
  if(!soundEditor) buzzerPlay(b.pin==BUTTON_A_PIN?SoundID::BUTTON_A:SoundID::BUTTON_B,now);
}
void buttonLong(ButtonReader &b,uint32_t now) {
  if(isIntroState(state)) return;
  notePlayedToday();
  if(b.pin==BUTTON_A_PIN) menuHome(now);
  else if(state==State::MENU) menuSelect(now);
  if(buzzerEnabled && buzzerVolume>0)
    buzzerPlay(b.pin==BUTTON_A_PIN?SoundID::BUTTON_A:SoundID::BUTTON_B,now);
}
void scanButton(ButtonReader &b,uint32_t now) {
  bool raw=(digitalRead(b.pin)==LOW);
  if(raw!=b.rawDown) {b.rawDown=raw;b.changed=now;}
  if(raw!=b.down && now-b.changed>=BUTTON_DEBOUNCE_MS) {
    b.down=raw;
    if(raw) {b.pressedAt=now;b.longDone=false;b.chordDone=false;}
    else if(!b.chordDone && !b.longDone && now-b.pressedAt<BUTTON_LONG_MS)
      buttonShort(b,now); // A partial long-hold isn't a short tap on release.
  }
}
void updateButtons(uint32_t now) {
  scanButton(buttonA,now);
  scanButton(buttonB,now);
  if(buttonA.down && buttonB.down) {
    if(!bothButtonsLatched && !isIntroState(state)) {
      bothButtonsLatched=true;
      buttonA.chordDone=true;buttonB.chordDone=true;
      buttonA.longDone=true;buttonB.longDone=true;
      pet.secrets|=(1U<<0); // hidden royal button combination
      markPetDirty();
      notePlayedToday();
      savePetIfNeeded(now,true);
      lastInteraction=now;
      setState(State::SECRET_REACTION,now);
      Serial.println(F("PIXO SECRET: button A + B"));
    }
  } else bothButtonsLatched=false;
  if(buttonA.down && !buttonA.longDone && !buttonA.chordDone &&
     now-buttonA.pressedAt>=BUTTON_LONG_MS) {
    buttonA.longDone=true;buttonLong(buttonA,now);
  }
  if(buttonB.down && !buttonB.longDone && !buttonB.chordDone) {
    const bool resetPage=inSettingsDetail() && settingsView==SettingsView::RESET;
    if(resetPage) {
      // Don't let an ordinary 750-ms hold erase pet memories.
      if(resetArmed && now-buttonB.pressedAt>=RESET_HOLD_MS) {
        buttonB.longDone=true;
        factoryResetPet();
      } else if(!resetArmed && now-buttonB.pressedAt>=BUTTON_LONG_MS) {
        // An unarmed long press must NOT become a B-short ARM on release.
        buttonB.longDone=true;
      }
    } else if(now-buttonB.pressedAt>=BUTTON_LONG_MS) {
      buttonB.longDone=true;buttonLong(buttonB,now);
    }
  }
}

// ===================== NETWORK + NTP ================================
// Called during first intro screen; polling is entirely non-blocking.
// Only WiFi.begin/configTime briefly issue calls; there is no delay loop.
void stopIntroWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
void completeIntroWiFi(WifiStage result, uint32_t now) {
  wifiStage=result;
  wifiFinishedAt=now;
  clockSynced=(result==WifiStage::OK);
  if(clockSynced) onClockReady();
  stopIntroWiFi();
  Serial.print(F("PIXO NTP result: "));
  Serial.println(clockSynced ? F("synced, WiFi off") : F("not synced, WiFi off"));
}
void beginIntroWiFi(uint32_t now) {
  wifiStageStarted=now;
  if (!WIFI_ENABLED || WIFI_SSID[0]=='\0') {
    wifiStage=WifiStage::NO_CONFIG;
    wifiFinishedAt=now;
    WiFi.mode(WIFI_OFF);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  wifiStage=WifiStage::CONNECTING;
}
void updateIntroWiFi(uint32_t now) {
  if (wifiStage==WifiStage::CONNECTING) {
    if (WiFi.status()==WL_CONNECTED) {
      configTime(TIMEZONE_GMT_OFFSET_SEC,TIMEZONE_DAYLIGHT_OFFSET_SEC,
                 NTP_SERVER_1,NTP_SERVER_2);
      wifiStage=WifiStage::SYNCING;
      wifiStageStarted=now;
      Serial.println(F("PIXO: WiFi connected; synchronizing date/time"));
    } else if (now-wifiStageStarted>=WIFI_CONNECT_TIMEOUT_MS) {
      completeIntroWiFi(WifiStage::CONNECT_FAILED,now);
    }
  } else if (wifiStage==WifiStage::SYNCING) {
    if (time(nullptr)>(time_t)1700000000) {
      completeIntroWiFi(WifiStage::OK,now);
    } else if (now-wifiStageStarted>=NTP_SYNC_TIMEOUT_MS) {
      completeIntroWiFi(WifiStage::NTP_FAILED,now);
    }
  }
}
bool wifiIntroDone() {
  return wifiStage==WifiStage::OK || wifiStage==WifiStage::NO_CONFIG ||
         wifiStage==WifiStage::CONNECT_FAILED || wifiStage==WifiStage::NTP_FAILED;
}
void drawIntroCredits(uint32_t now) {
  display.setTextSize(1);
  display.setCursor(28,0);display.print(F("DEVELOPED BY"));
  display.setTextSize(2);
  display.setCursor(40,10);display.print(F("ARUN"));
  display.setCursor(28,27);display.print(F("YOKESH"));
  display.setTextSize(1);
  display.setCursor(12,45);
  switch(wifiStage) {
    case WifiStage::CONNECTING: display.print(F("WiFi: connecting"));break;
    case WifiStage::SYNCING:
      if (now-wifiStageStarted<500) display.print(F("WiFi connected!"));
      else display.print(F("Syncing date/time"));
      break;
    case WifiStage::OK: display.print(F("Time set! WiFi off"));break;
    case WifiStage::NO_CONFIG: display.print(F("Set WiFi in code"));break;
    case WifiStage::CONNECT_FAILED: display.print(F("WiFi not found"));break;
    case WifiStage::NTP_FAILED: display.print(F("NTP sync failed"));break;
    default: display.print(F("Starting WiFi"));break;
  }
  display.drawRoundRect(12,56,104,6,2,SSD1306_WHITE);
  // A waiting indicator, not an invented network packet percentage.
  uint32_t t=now-stateStarted;
  int width=0;
  if (wifiStage==WifiStage::CONNECTING)
    width=8+(int)(58*progress(now-wifiStageStarted,WIFI_CONNECT_TIMEOUT_MS));
  else if (wifiStage==WifiStage::SYNCING)
    width=66+(int)(31*progress(now-wifiStageStarted,NTP_SYNC_TIMEOUT_MS));
  else if (wifiIntroDone()) width=100;
  if (width>0) display.fillRoundRect(14,58,width,2,1,SSD1306_WHITE);
  if (!wifiIntroDone() && ((t/250)%2)==0) tinySparkle(116,48,2);
}
void drawPushIntro(uint32_t dt) {
  const uint32_t enterMs=330;
  const uint32_t pushMs=865;
  int lx,rx,screenX=16;
  if (dt<enterMs) {
    float p=ease(progress(dt,enterMs));
    lx=mixInt(169,125,p);
    rx=mixInt(215,170,p);
  } else {
    float p=ease(progress(dt-enterMs,pushMs));
    screenX=mixInt(16,-145,p);
    lx=mixInt(125,LEFT_EYE_X,p);
    rx=mixInt(170,RIGHT_EYE_X,p);
  }
  if (screenX>-110) {
    display.setTextSize(4);
    display.setCursor(screenX,16);
    display.print(F("PIXO"));
  }
  int hop=dt>1195 ? (int)roundf(4*sinf((float)(dt-1195)*.025f) *
                                  (1.0f-ease(progress(dt-1195,235)))) : 0;
  // Draw two rolling eyes as they shoulder the PIXO title off-screen.
  oneEye(lx,32-hop,0.92f,-2);
  oneEye(rx,32-hop,0.92f,2);
  if (dt>enterMs && dt<1120) tinySparkle(117,47,2);
}

// ====================== NORMAL ANIMATIONS ============================
void scheduleBlink(uint32_t now) {
  nextBlink = now + (uint32_t)random(1800, 5201);
}
void scheduleExpressions(uint32_t now) {
  nextFace = now + (uint32_t)random(FACE_GAP_MIN_MS, FACE_GAP_MAX_MS+1);
  nextHearts = now + (uint32_t)random(HEART_GAP_MIN_MS, HEART_GAP_MAX_MS+1);
  nextBlush = now + (uint32_t)random(9000,22000);
}
void resetNatural(uint32_t now) {
  blink = Blink::READY;
  doDoubleBlink = false;
  scheduleBlink(now);
  gazeX=gazeY=fromX=fromY=targetX=targetY=0;
  gazeStarted=now;
  gazeTravelMs=380;
  nextLook=now+(uint32_t)random(900,1800);
}
void animateBlink(uint32_t now) {
  const uint32_t dt = now - blinkStarted;
  switch (blink) {
    case Blink::READY:
      if (reached(now,nextBlink)) {
        blink = Blink::CLOSING;
        blinkStarted = now;
        doDoubleBlink = random(100) < 23;
      }
      break;
    case Blink::CLOSING:
      if (dt >= BLINK_SHUT_MS) { blink=Blink::OPENING; blinkStarted=now; }
      break;
    case Blink::OPENING:
      if (dt >= BLINK_OPEN_MS) {
        blinkStarted=now;
        if (doDoubleBlink) {
          blink=Blink::DOUBLE_PAUSE;
          doDoubleBlink=false;
        } else {
          blink=Blink::READY;
          scheduleBlink(now);
        }
      }
      break;
    case Blink::DOUBLE_PAUSE:
      if (dt >= DOUBLE_BLINK_PAUSE_MS) {
        blink=Blink::CLOSING;
        blinkStarted=now;
      }
      break;
  }
}
float blinkOpenness(uint32_t now) {
  if (blink==Blink::CLOSING)
    return 1.0f-ease(progress(now-blinkStarted, BLINK_SHUT_MS));
  if (blink==Blink::OPENING)
    return ease(progress(now-blinkStarted, BLINK_OPEN_MS));
  return 1.0f;
}
void animateGaze(uint32_t now) {
  float t = ease(progress(now-gazeStarted, gazeTravelMs));
  gazeX=fromX+(targetX-fromX)*t;
  gazeY=fromY+(targetY-fromY)*t;
  if (reached(now,nextLook) && now-gazeStarted>=gazeTravelMs) {
    fromX=gazeX;
    fromY=gazeY;
    const int choice = random(100);
    targetX=choice<30 ? -6 : (choice<60 ? 6 : 0);
    targetY=(float)random(-2,3);
    gazeStarted=now;
    gazeTravelMs=(uint32_t)random(280,540);
    nextLook=now+gazeTravelMs+(uint32_t)random(900,2400);
  }
}
void drawRelaxed(uint32_t dt) {
  float openness, gazeWeight;
  if (dt<550) {
    float t=ease(progress(dt,550));
    openness=1-0.45f*t;
    gazeWeight=1-t;
  } else if (dt<1450) {
    openness=0.55f-0.025f*sinf(progress(dt-550,900)*3.14159265f);
    gazeWeight=0;
  } else {
    openness=0.55f+0.45f*ease(progress(dt-1450,550));
    gazeWeight=0;
  }
  eyes(openness,expressionGazeX*gazeWeight,0,
       expressionGazeY*gazeWeight);
}
void drawBright(uint32_t dt) {
  float t=dt<340 ? ease(progress(dt,340)) :
          (dt<1260 ? 1.0f : 1.0f-ease(progress(dt-1260,540)));
  eyes(1.0f,expressionGazeX*(1.0f-.25f*t),0,
       expressionGazeY*(1.0f-t)-1.6f*t,t);
}
void drawPet(uint32_t dt, uint32_t now) {
  // The ^ ^ shape appears on the FIRST frame of the touch, with no
  // 100-200 ms eye-close transition that would make short taps feel late.
  // The same movement continues indefinitely during a held touch.
  const float time = (float)(now - stateStarted);
  const float sway = 4.3f * sinf(time * 0.0092f);      // left/right ^ ^ movement
  const float bob = 2.8f * sinf(time * 0.0150f);       // playful bounce
  const uint32_t tapAge = now - petTapAt;
  const float tapKick = tapAge < 220 ?
      3.0f * (1.0f - ease(progress(tapAge, 220))) *
      sinf((float)tapAge * 0.050f) : 0.0f;
  // Tiny playful opening bounce, but always visibly inverted-V.
  const float opening = 0.78f + 0.22f * ease(progress(dt, 70));
  happyPetEyes(opening, bob + tapKick, sway);
  drawPetCuteFx(now);
}
void drawDrowsy(uint32_t dt) {
  float t=progress(dt,DROWSY_MS);
  float open;
  if (t<.48f) open=1-.72f*ease(t/.48f);
  else if (t<.72f) open=.28f+.18f*ease((t-.48f)/.24f);
  else open=.46f*(1-ease((t-.72f)/.28f));
  float center=1.0f-ease(progress(dt,600));
  eyes(open,gazeX*center,0,gazeY*center);
}
void drawDreamIcon(uint32_t dt) {
  // The dream changes every ~4.2 seconds, so PIXO feels alive in sleep.
  // Tiny connected bubbles rise toward one larger thought bubble.
  int bob=(int)roundf(2*sinf((float)dt*.003f));
  display.drawCircle(60,25,2,SSD1306_WHITE);
  display.drawCircle(64,21,3,SSD1306_WHITE);
  display.drawCircle(69,10+bob,12,SSD1306_WHITE);
  switch ((dt/4200UL)%3) {
    case 0: { // fish outline, fin, tail and eye
      oval(70,10+bob,5,3,SSD1306_WHITE);
      display.fillTriangle(64,10+bob,59,6+bob,59,14+bob,SSD1306_WHITE);
      display.drawPixel(72,9+bob,SSD1306_BLACK);
      break;
    }
    case 1: { // birthday cake with candle
      display.drawRect(65,8+bob,10,7,SSD1306_WHITE);
      display.drawFastHLine(64,8+bob,12,SSD1306_WHITE);
      display.drawLine(69,7+bob,69,4+bob,SSD1306_WHITE);
      display.drawPixel(69,3+bob,SSD1306_WHITE);
      break;
    }
    default: { // heart
      tinyHeart(69,9+bob,6);
      break;
    }
  }
}
void drawSleeping(uint32_t dt) {
  for (int i=0; i<2; ++i) {
    int x=i?RIGHT_EYE_X:LEFT_EYE_X;
    thickLine(x-12,33,x,35);
    thickLine(x,35,x+12,33);
  }
  // No sudden position jump: smooth continuous Zzz bob.
  float wave=sinf(((float)(dt%2600UL)/2600.0f)*6.2831853f);
  int rise=(int)roundf(2.0f*wave);
  display.setTextSize(1);
  display.setCursor(99,6-rise);
  display.print(F("Zzz"));
  drawDreamIcon(dt);
}
void drawWaking(uint32_t dt) {
  uint32_t popDt=dt>20 ? dt-20 : 0;
  float open=ease(progress(popDt,WAKE_POP_MS));
  float damp=1.0f-ease(progress(dt,WAKE_SHAKE_MS));
  float shakeX=10.0f*damp*sinf((float)dt*.052f);
  float shakeY=damp*(-2.0f+2.0f*sinf((float)dt*.075f));
  float shock=0;
  if (dt>85 && dt<420)
    shock=ease(progress(dt-85,95)) *
          (1.0f-ease(progress(dt>290 ? dt-290 : 0,130)));
  eyes(open,0,shakeX,shakeY,shock);
}
void drawLogo(uint32_t dt, bool full) {
  float t=full ? 1.0f : ease(progress(dt,INTRO_BOX_MS));
  int x=mixInt(33,3,t), y=mixInt(16,2,t);
  display.drawRoundRect(x,y,128-2*x,64-2*y,6,SSD1306_WHITE);
  if (full) display.drawRoundRect(6,5,116,54,5,SSD1306_WHITE);
  if (t>.72f) {
    display.setTextSize(4);
    display.setCursor(16,16);
    display.print(F("PIXO"));
  }
}
void drawCredits() {
  display.setTextSize(1);
  display.setCursor(28,10); display.print(F("DEVELOPED BY"));
  display.setTextSize(2);
  display.setCursor(40,25); display.print(F("ARUN"));
  display.setCursor(28,44); display.print(F("YOKESH"));
}

// ====================== STATE / TOUCH LOGIC ==========================
void setState(State next, uint32_t now) {
  const State old=state;
  state=next;
  stateStarted=now;
  if(next!=old) {
    switch(next) {
      case State::INTRO_BOX: buzzerPlay(SoundID::STARTUP,now);break;
      case State::INTRO_BIRTHDAY: buzzerPlay(SoundID::BIRTHDAY,now);break;
      case State::WAKING: buzzerPlay(SoundID::WAKE,now);break;
      case State::DROWSY: buzzerPlay(SoundID::SLEEP,now);break;
      case State::MUSIC: buzzerPlay(SoundID::MUSIC,now);break;
      case State::SECRET_REACTION: buzzerPlay(SoundID::SECRET,now);break;
      case State::SLEEP:
        buzzerHalt();
        nextDreamChirp=now+(uint32_t)random(25000,45001);
        break;
      default:break;
    }
  }

  if (next==State::NORMAL) {
    // Only reboot/wake finishing starts a new idle window. An automatic
    // expression or a pet reaction cannot extend the 60-second inactivity.
    if (old==State::INTRO_PROFILE || old==State::INTRO_BIRTHDAY ||
        old==State::WAKING)
      lastInteraction=now;
    resetNatural(now);
  }
  if (next==State::RELAXED || next==State::BRIGHT || next==State::HEARTS ||
      next==State::PET || next==State::WINK || next==State::MUSIC ||
      next==State::EMBARRASSED || next==State::BALL_KICK ||
      next==State::PEEK || next==State::DANCE ||
      next==State::UPSIDE_DOWN || next==State::STARRY) {
    expressionGazeX=gazeX;
    expressionGazeY=gazeY;
  }
  if (next==State::RELAXED || next==State::BRIGHT || next==State::WINK ||
      next==State::MUSIC || next==State::EMBARRASSED || next==State::BALL_KICK ||
      next==State::PEEK || next==State::DANCE ||
      next==State::UPSIDE_DOWN || next==State::STARRY)
    nextFace=now+(uint32_t)random(FACE_GAP_MIN_MS,FACE_GAP_MAX_MS+1);
  if (next==State::HEARTS)
    nextHearts=now+(uint32_t)random(HEART_GAP_MIN_MS,HEART_GAP_MAX_MS+1);
  // Keep a real per-animation history to calculate "Favourite reaction".
  if(next!=old) {
    switch(next) {
      case State::RELAXED: recordReaction(R_RELAXED);break;
      case State::BRIGHT: recordReaction(R_BRIGHT);break;
      case State::WINK: recordReaction(R_WINK);break;
      case State::MUSIC: recordReaction(R_MUSIC);break;
      case State::EMBARRASSED: recordReaction(R_EMBARRASSED);break;
      case State::BALL_KICK: recordReaction(R_FOOTBALL);break;
      case State::PEEK: recordReaction(R_PEEK);break;
      case State::DANCE: recordReaction(R_DANCE);break;
      case State::UPSIDE_DOWN: recordReaction(R_UPSIDE);break;
      case State::STARRY: recordReaction(R_STARRY);break;
      case State::HEARTS: recordReaction(R_HEARTS);break;
      default:break;
    }
  }
}
// Power-on ONLY. Touch cannot call or schedule this function.
void startIntroAtBoot(uint32_t now) {
  lastInteraction=now;
  setState(State::INTRO_CREDITS,now);
}

void startPet(uint32_t now) {
  const bool wasAlreadyPetting = state == State::PET;
  lastInteraction = now;
  petTapAt = now;
  spawnPetHearts(now);
  if (!wasAlreadyPetting) {
    setState(State::PET, now);
  }
  // On every additional tap, keep the current ^ ^ eyes moving smoothly.
  // No intro and no forced normal-eye frame between taps.
  scheduleExpressions(now);
}
void updateState(uint32_t now) {
  uint32_t dt=now-stateStarted;
  switch (state) {
    case State::INTRO_CREDITS:
      if (dt>=INTRO_FIRST_SCREEN_MIN_MS && wifiIntroDone() &&
          now-wifiFinishedAt>=WIFI_STATUS_HOLD_MS)
        setState(State::INTRO_BOX,now);
      break;
    case State::INTRO_BOX:
      if (dt>=INTRO_BOX_MS) setState(State::INTRO_NAME,now);
      break;
    case State::INTRO_NAME:
      if (dt>=INTRO_NAME_MS) setState(State::INTRO_PUSH,now);
      break;
    case State::INTRO_PUSH:
      if (dt>=INTRO_PUSH_MS) setState(State::INTRO_OPEN,now);
      break;
    case State::INTRO_OPEN:
      if (dt>=INTRO_OPEN_MS) setState(State::INTRO_HELLO,now);
      break;
    case State::INTRO_HELLO:
      if(dt>=INTRO_HELLO_MS) setState(State::INTRO_PROFILE,now);
      break;
    case State::INTRO_PROFILE:
      if(dt>=INTRO_PROFILE_MS) {
        if(anniversaryQueued) {
          anniversaryQueued=false;
          setState(State::INTRO_BIRTHDAY,now);
        } else {setState(State::NORMAL,now);scheduleExpressions(now);}
      }
      break;
    case State::INTRO_BIRTHDAY:
      if(dt>=INTRO_BIRTHDAY_MS) {
        setState(State::NORMAL,now);scheduleExpressions(now);
      }
      break;
    case State::NORMAL: {
      if(anniversaryQueued) {
        anniversaryQueued=false;
        setState(State::INTRO_BIRTHDAY,now);
        break;
      }
      const uint32_t idle=now-lastInteraction;
      if (AUTO_SLEEP && stableTouch==TOUCH_IDLE && idle>=IDLE_SLEEP_MS) {
        setState(State::DROWSY,now);
        break;
      }
      animateBlink(now);
      animateGaze(now);
      if (reached(now,nextBlush)) {
        blushShowing=true;
        blushStarted=now;
        nextBlush=now+(uint32_t)random(11000,23000);
      }
      if (blushShowing && now-blushStarted>=RANDOM_BLUSH_MS)
        blushShowing=false;
      if (stableTouch==TOUCH_IDLE && blink==Blink::READY &&
          (!AUTO_SLEEP || idle+HEART_MS+350 < IDLE_SLEEP_MS)) {
        if (HEART_BALLOONS && reached(now,nextHearts)) {
          setState(State::HEARTS,now);
        } else if (RANDOM_EXPRESSIONS && reached(now,nextFace)) {
          int choice=random(100);
          if (choice<13) setState(State::RELAXED,now);
          else if (choice<24) setState(State::BRIGHT,now);
          else if (choice<35) setState(State::WINK,now);
          else if (choice<47) setState(State::MUSIC,now);
          else if (choice<57) setState(State::EMBARRASSED,now);
          else if (choice<67) setState(State::BALL_KICK,now);
          else if (choice<77) setState(State::PEEK,now);
          else if (choice<86) setState(State::DANCE,now);
          else if (choice<93) setState(State::UPSIDE_DOWN,now);
          else setState(State::STARRY,now);
        }
      }
      break;
    }
    case State::RELAXED:
      if (dt>=RELAXED_MS) setState(State::NORMAL,now);
      break;
    case State::BRIGHT:
      if (dt>=BRIGHT_MS) setState(State::NORMAL,now);
      break;
    case State::HEARTS:
      if (dt>=HEART_MS) setState(State::NORMAL,now);
      break;
    case State::WINK:
      if (dt>=WINK_MS) setState(State::NORMAL,now);
      break;
    case State::MUSIC:
      if (dt>=MUSIC_MS) setState(State::NORMAL,now);
      break;
    case State::EMBARRASSED:
      if (dt>=EMBARRASSED_MS) setState(State::NORMAL,now);
      break;
    case State::BALL_KICK:
      if (dt>=BALL_KICK_MS) setState(State::NORMAL,now);
      break;
    case State::PEEK:
      if(dt>=PEEK_MS) setState(State::NORMAL,now);
      break;
    case State::DANCE:
      if(dt>=DANCE_MS) setState(State::NORMAL,now);
      break;
    case State::UPSIDE_DOWN:
      if(dt>=UPSIDE_DOWN_MS) setState(State::NORMAL,now);
      break;
    case State::STARRY:
      if(dt>=STARRY_MS) setState(State::NORMAL,now);
      break;
    case State::SECRET_REACTION:
      if(dt>=SECRET_CELEBRATE_MS) {setState(State::NORMAL,now);scheduleExpressions(now);}
      break;
    case State::MENU:
      // Menu is controlled by GPIO18/19. Idle sleep is suppressed in menu.
      break;
    case State::CALENDAR:
      // Release is handled in updateTouch(); no intro/restart or blocking.
      break;
    case State::PET: {
      // IMPORTANT: an external TTP223 can remain HIGH for 2-5 seconds
      // after a short tap. Never let the sensor HIGH or recurring heart
      // particles extend the facial-expression timer indefinitely.
      const uint32_t petAge = now - petTapAt;
#if USE_NATIVE_ESP32_TOUCH
      const bool realTouchHeld = stableTouch == TOUCH_ACTIVE;
#else
      const bool realTouchHeld = CONTINUE_PET_WHILE_TOUCH_HIGH &&
                                 stableTouch == TOUCH_ACTIVE;
#endif
      if (!realTouchHeld && petAge >= PET_AFTER_TAP_MS) {
        setState(State::NORMAL, now);
        scheduleExpressions(now);
      }
      // Default TTP223: ALWAYS stop after ~1.15 seconds (not 5 seconds),
      // despite the LED / OUT staying HIGH after the finger is removed.
      break;
    }
    case State::DROWSY:
      if (dt>=DROWSY_MS) setState(State::SLEEP,now);
      break;
    case State::SLEEP:
      if(reached(now,nextDreamChirp)) {
        buzzerPlay(SoundID::DREAM,now);
        nextDreamChirp=now+(uint32_t)random(25000,45001);
      }
      break;
    case State::WAKING:
      if (dt>=WAKE_MS) {
        if (petAfterWake) {
          petAfterWake=false;
          startPet(now);  // The wake-up tap also gets the cute ^ ^ reaction.
        } else {
          setState(State::NORMAL,now);
          scheduleExpressions(now);
        }
      }
      break;
  }
}
// Touch detection is separate from animation timing: never block the loop.
// Both input choices feed the same quick press + continuous-held response.
uint8_t readTouchLevel() {
#if USE_NATIVE_ESP32_TOUCH
  // Native GPIO32 T9 on CLASSIC ESP32; bare electrode, NO TTP223 board.
  // touchRead value falls when a finger touches the electrode.
  return touchRead(TOUCH_PIN) < nativeTouchThreshold ? TOUCH_ACTIVE : TOUCH_IDLE;
#else
  // TTP223 digital output. HIGH=touch in default non-toggle mode.
  return digitalRead(TOUCH_PIN);
#endif
}
void updateTouch(uint32_t now) {
  const uint8_t reading = readTouchLevel();
  if (reading != lastRawTouch) touchChangedAt = now;
  lastRawTouch = reading;

  if (reading != stableTouch && now - touchChangedAt >= DEBOUNCE_MS) {
    stableTouch = reading;
    if (stableTouch == TOUCH_ACTIVE) {
      touchDownAt=now;
      calendarHoldConsumed=false;
      ++touchCount;
      lastInteraction = now;
      if(state!=State::INTRO_CREDITS && state!=State::INTRO_BOX &&
         state!=State::INTRO_NAME && state!=State::INTRO_PUSH &&
         state!=State::INTRO_OPEN && state!=State::INTRO_HELLO &&
         state!=State::INTRO_PROFILE && state!=State::INTRO_BIRTHDAY)
        recordPetTouch();
      if (TOUCH_DEBUG) {
        Serial.print(F("TOUCH: DOWN #"));
        Serial.println(touchCount);
      }
      if (state==State::INTRO_CREDITS || state==State::INTRO_BOX ||
          state==State::INTRO_NAME || state==State::INTRO_PUSH ||
          state==State::INTRO_OPEN || state==State::INTRO_HELLO ||
          state==State::INTRO_PROFILE || state==State::INTRO_BIRTHDAY) {
        // Let startup finish; initial greeting cannot be reset by touch.
      } else if (state == State::SLEEP || state == State::DROWSY) {
        petAfterWake = true;
        setState(State::WAKING, now);
      } else if (state == State::WAKING) {
        // Do not interrupt shaking eyes. Its end will run startPet().
        petAfterWake = true;
      } else {
        startPet(now);
      }
      // A short happy chirp for petting; never replays any intro.
      if((state==State::PET) && !petAfterWake)
        buzzerPlay(SoundID::PET,now);
    } else {
      if (state==State::CALENDAR) {
        setState(State::NORMAL,now);
        scheduleExpressions(now);
      }
      if (TOUCH_DEBUG) Serial.println(F("TOUCH: UP"));
    }
  }

  if (stableTouch == TOUCH_ACTIVE) {
    if (CALENDAR_ON_LONG_TOUCH && !calendarHoldConsumed &&
        state!=State::CALENDAR && state!=State::INTRO_CREDITS &&
        state!=State::INTRO_BOX && state!=State::INTRO_NAME &&
        state!=State::INTRO_PUSH && state!=State::INTRO_OPEN &&
        state!=State::INTRO_HELLO && state!=State::INTRO_PROFILE &&
        state!=State::INTRO_BIRTHDAY && state!=State::MENU &&
        state!=State::SECRET_REACTION && state!=State::DROWSY && state!=State::SLEEP &&
        state!=State::WAKING) {
#if USE_NATIVE_ESP32_TOUCH
      const uint32_t longHoldMs=NATIVE_CALENDAR_HOLD_MS;
#else
      const uint32_t longHoldMs=TTP223_CALENDAR_HOLD_MS;
#endif
      if (now-touchDownAt>=longHoldMs) {
        calendarHoldConsumed=true;
        setState(State::CALENDAR,now);
        Serial.println(F("PIXO: calendar hold"));
      }
    }
    // Keep idle sleep from interrupting the reaction while OUT is active.
    // Repeating visual particles NEVER extends the PET facial-expression timer.
    lastInteraction = now;
    if (state == State::PET && now - petHoldBurstAt >= HELD_HEART_EVERY_MS) {
#if USE_NATIVE_ESP32_TOUCH
      const bool canRepeat = true;
#else
      const uint32_t petAge = now - petTapAt;
      // During the 1-second auto-return reaction, allow one or two extra
      // hearts but don't start particles right before we return to normal.
      const bool canRepeat = CONTINUE_PET_WHILE_TOUCH_HIGH ||
                             petAge + PET_HEART_LIFETIME_MS < PET_AFTER_TAP_MS;
#endif
      if (canRepeat) {
        spawnPetHearts(now);
        if (TOUCH_DEBUG) Serial.println(F("TOUCH: HEART BURST"));
      }
    }
  }
}

// ====================== RENDER + HARDWARE ============================
void render(uint32_t now) {
  display.clearDisplay();   // RAM only; avoids hardware clear/display flicker
  display.setTextColor(SSD1306_WHITE);
  uint32_t dt=now-stateStarted;
  switch (state) {
    case State::INTRO_BOX: drawLogo(dt,false); break;
    case State::INTRO_NAME: drawLogo(dt,true); break;
    case State::INTRO_CREDITS: drawIntroCredits(now); break;
    case State::INTRO_PUSH: drawPushIntro(dt); break;
    case State::INTRO_OPEN: eyes(0.70f+0.30f*ease(progress(dt,INTRO_OPEN_MS))); break;
    case State::INTRO_HELLO: drawIntroHello(dt);break;
    case State::INTRO_PROFILE: drawIntroProfile(dt);break;
    case State::INTRO_BIRTHDAY: drawBirthday(dt);break;
    case State::NORMAL: {
      float bob=.7f*sinf((float)(now%6000UL)*.00104719755f);
      eyes(blinkOpenness(now),gazeX,0,gazeY+bob);
      if (blushShowing) drawBlush();
      drawAccessoryOverlay(pet.accessory);
      if (blink != Blink::READY && ((now/110)%2)==0) {
        tinySparkle(13,18,2); tinySparkle(115,18,2);
      }
      break;
    }
    case State::RELAXED: drawRelaxed(dt); break;
    case State::BRIGHT: drawBright(dt); break;
    case State::HEARTS: drawHeartBalloons(dt); break;
    case State::WINK: drawWink(dt); break;
    case State::MUSIC: drawMusic(dt); break;
    case State::EMBARRASSED: drawEmbarrassed(dt); break;
    case State::BALL_KICK: drawBallKick(dt); break;
    case State::PEEK:drawCuriousPeek(dt);break;
    case State::DANCE:drawHappyDance(dt);break;
    case State::UPSIDE_DOWN:drawUpsideDown(dt);break;
    case State::STARRY:drawStarry(dt);break;
    case State::MENU:drawMenu(now);break;
    case State::SECRET_REACTION:drawSecret(dt);break;
    case State::CALENDAR: drawCalendar(now); break;
    case State::PET: drawPet(dt,now); break;
    case State::DROWSY: drawDrowsy(dt); break;
    case State::SLEEP: drawSleeping(dt); break;
    case State::WAKING: drawWaking(dt); break;
  }
  uint8_t* current=display.getBuffer();
  if (!hasPreviousScreen || memcmp(current,previousScreen,1024)!=0) {
    display.display();
    memcpy(previousScreen,current,1024);
    hasPreviousScreen=true;
  }
}
void setup() {
  Serial.begin(115200);
#if USE_NATIVE_ESP32_TOUCH
  // Calibrate untouched GPIO32 electrode. Avoid contact during boot.
  // The sensor's OUT must NOT be attached when using native touchRead().
  uint32_t total = 0;
  for (uint8_t i = 0; i < 24; ++i) {
    total += touchRead(TOUCH_PIN);
    delay(7);                // setup-time electrode baseline only
  }
  uint16_t baseline = (uint16_t)(total / 24);
  nativeTouchThreshold = (uint16_t)(((uint32_t)baseline *
                                     NATIVE_TOUCH_PERCENT_OF_BASELINE) / 100);
  if (TOUCH_DEBUG) {
    Serial.print(F("PIXO native touch baseline: ")); Serial.println(baseline);
    Serial.print(F("PIXO native touch threshold: ")); Serial.println(nativeTouchThreshold);
    Serial.println(F("Do NOT connect external TTP223 OUT in native mode."));
  }
#else
  // TTP223 actively drives its OUT HIGH/LOW; no internal pull-up.
  pinMode(TOUCH_PIN, INPUT);
  if (TOUCH_DEBUG) {
    Serial.println(F("PIXO TTP223: OUT->GPIO32, VCC->3V3, GND->GND"));
    Serial.println(F("PIXO touch: jumper B/TOG must be OPEN (momentary mode)"));
    Serial.println(F("PIXO AUTO-RETURN: normal eyes in ~1.15 s, even if LED remains ON"));
  }
#endif
  touchChangedAt = millis();
  pinMode(BUTTON_A_PIN, INPUT_PULLUP); // Button A -> GPIO18 and GND
  pinMode(BUTTON_B_PIN, INPUT_PULLUP); // Button B -> GPIO19 and GND
  Wire.begin(OLED_SDA,OLED_SCL);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR,true,false)) {
    Serial.println(F("PIXO: OLED not found. Check wiring and address."));
    while (true) delay(1000);  // only on display-initialization failure
  }
  display.setTextWrap(false);
  randomSeed(esp_random());
  loadPetMemory();
  buzzerInit();  // GPIO27, separate from TTP223 on GPIO32
  updateDisplayContrast(); // use saved level immediately, even during intro
  startIntroAtBoot(millis());
  beginIntroWiFi(millis());
  lastFrame=millis()-FRAME_MS;
}
void loop() {
  uint32_t now=millis();
  updateIntroWiFi(now);     // Startup-only network time sync; Wi-Fi then OFF
  updateDisplayContrast();  // only sends commands when contrast actually changes
  updateButtons(now);        // 2 tactile menu keys, non-blocking and debounced
  buzzerUpdate(now);       // nonblocking PWM tone sequencer
  updateTouch(now);          // Existing touch petting behaviour
  checkPetDateDuringRuntime(now);
  updateState(now);
  savePetIfNeeded(now);      // NVS wear-aware periodic save
  if (now-lastFrame>=FRAME_MS) {
    lastFrame=now;
    render(now);
  }
}
