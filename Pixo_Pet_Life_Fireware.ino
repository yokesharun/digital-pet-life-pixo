/*
 PIXO Pet Life v6 - longer kawaii shows, expressive sleepy face, feeding,
 simplified 250Hz beep, first-start-of-day greetings and no touch sensor.
 Classic ESP32 38-pin + SSD1306 128x64 I2C. GPIO25 SDA, GPIO26 SCL;
 A on GPIO18->GND, B on GPIO19->GND; passive piezo on GPIO27->GND.
 No connection is required on GPIO32. All pre-existing pet NVS data is kept.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>
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
// Use a low-current PASSIVE piezo for fixed 250 Hz output.
// GPIO32 is now UNUSED; old touch sensor has been completely removed.
// 2-wire passive piezo: + -> GPIO27; - -> shared GND.
// Use an NPN driver for modules drawing more current than a GPIO can safely source.
constexpr uint8_t BUZZER_PIN = 27;
constexpr bool BUZZER_ENABLED_DEFAULT = true;
constexpr uint16_t BUZZER_FIXED_HZ = 250; // Base PWM; secret jingles vary pitch.
constexpr bool TEST_MODE_DEFAULT = false; // Developer secret-preview mode
constexpr bool WEATHER_ENABLED = true;
constexpr uint8_t BUZZER_LEDC_CHANNEL = 5; // Arduino-ESP32 2.x only
constexpr uint8_t BUZZER_PWM_BITS = 10;    // 0..1023 PWM duty
// All melodies are original, with sound ON/OFF only (no software volume slider).
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

constexpr uint32_t INTRO_HELLO_MS = 5500;
constexpr uint32_t INTRO_PROFILE_MS = 5200;
constexpr uint32_t INTRO_BIRTHDAY_MS = 14600;
constexpr uint32_t PEEK_MS = 2700;
constexpr uint32_t DANCE_MS = 2850;
constexpr uint32_t UPSIDE_DOWN_MS = 3000;
constexpr uint32_t STARRY_MS = 2350;
constexpr uint32_t SECRET_CELEBRATE_MS = 12550; // old 2550 ms + 10 seconds
const char TIMEZONE_NAME[] = "Asia/Kolkata";
const char TIMEZONE_POSIX[] = "IST-5:30"; // No DST (UTC+05:30)
const char NTP_SERVER_1[] = "pool.ntp.org";
const char NTP_SERVER_2[] = "time.google.com";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 9000;
constexpr uint32_t NTP_SYNC_TIMEOUT_MS = 5500;
constexpr uint32_t WIFI_STATUS_HOLD_MS = 1300;
constexpr uint32_t INTRO_FIRST_SCREEN_MIN_MS = 3300;
// Removed TTP223 and GPIO32 capacitive touch handling entirely.
constexpr int OLED_SDA    = 25;
constexpr int OLED_SCL    = 26;
// GPIO32 deliberately left unconfigured; no touch component required.
constexpr uint8_t OLED_ADDR = 0x3C;         // alternative is often 0x3D
constexpr uint32_t PET_HEART_LIFETIME_MS = 750;
constexpr uint32_t FRAME_MS = 33;           // about 30 FPS
// Touch-event constants removed. Eye, heart and pet animations are retained.
constexpr bool AUTO_SLEEP = true;
constexpr uint32_t IDLE_SLEEP_MS = 600000;  // 10 minutes of inactivity
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
constexpr uint32_t INTRO_NAME_MS = 2300;
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
   INTRO_HELLO, INTRO_PROFILE, INTRO_BIRTHDAY, DAILY_GREETING,
  NORMAL, RELAXED, BRIGHT, HEARTS, PET, DROWSY, SLEEP, WAKING,
  WINK, MUSIC, EMBARRASSED, BALL_KICK, CALENDAR,
  PEEK, DANCE, UPSIDE_DOWN, STARRY, MENU, SECRET_REACTION,
  WEATHER, FEEDING
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

// No touch sensor is connected.
uint32_t petTapAt=0;
uint32_t petHoldBurstAt=0;
bool petAfterWake=false;
// Kept petting animation capability; the physical touch input no longer exists.

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
  "My PIXO", "Calendar", "Battery", "Secrets", "Wardrobe", "Weather", "Settings"
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
bool editBuzzerEnabled = BUZZER_ENABLED_DEFAULT;
bool contrastInitialized = false;
uint8_t lastAppliedContrast = 0;

enum class SettingsView : uint8_t { LIST, BRIGHTNESS, AUTO_NIGHT, BUZZER_ENABLE, CLOCK, OWNER, MEMORY, TEST_SECRETS, RESET };
SettingsView settingsView = SettingsView::LIST;
uint8_t settingsIndex = 0;
uint8_t wardrobePreview = 0; // B tap browses; B long equips and saves
bool resetArmed = false;
const char* const SETTINGS_ITEMS[] = {
  "Brightness", "Auto night", "Buzzer", "Clock sync",
  "Pet owner", "Pet memory", "Test secrets", "Factory reset"
};
const char* const SETTINGS_SUBTITLES[] = {
  "My little glow", "Dreamy night glow", "Cute jingles ON/OFF",
  "My cozy time zone", "My favourite human",
  "Our story is safe", "Peek at magic!", "A fresh new start"
};
constexpr uint8_t SETTINGS_COUNT = sizeof(SETTINGS_ITEMS)/sizeof(SETTINGS_ITEMS[0]);
constexpr uint32_t CALENDAR_SHOW_MS=3000, WEATHER_SHOW_MS=3000, FEED_MS=4300;
constexpr uint32_t HUNGER_INTERVAL_MS=300000; // ask every ~5 minutes
constexpr uint32_t HUNGER_PROMPT_MS=30000;   // dismiss unfed request in 30 s
constexpr uint32_t GREETING_SHOW_MS=5600;
constexpr uint8_t SECRET_COUNT=7;
enum SecretIndex:uint8_t { SEC_PEEK, SEC_BIRTHDAY, SEC_ALIEN, SEC_ANGEL,
 SEC_GHOST, SEC_ROYAL, SEC_FRIEND };
const char* const SECRET_NAMES[]={"PEEKABOO!","PIXO BIRTHDAY","ALIEN INVASION", "ANGEL MODE", "MIDNIGHT GHOST", "ROYAL PIXO", "BEST FRIEND"};
const char* const SECRET_HINTS[][3]={
 {"Press A + B", "Little hand surprise", "A cute sparkle"},
 {"Birth anniversary", "Cake and confetti", "Every year"},
 {"Very rare event", "After weeks of use", "One eye flies away"},
 {"365 pet days", "Little halo+wings", "One year together"},
 {"A + B at 00:00", "Until 00:15 IST", "A floating ghost"},
 {"7,14,30,90,180,", "270 pet days", "A royal crown"},
 {"365 pet days", "A bee stays nearby", "Forever friends"}
};
enum class SecretAnim:uint8_t{PEEK,ALIEN,ANGEL,GHOST,ROYAL,FRIEND};
SecretAnim secretAnim=SecretAnim::PEEK;
uint8_t secretPage=0,royalMilestoneSaved=0;
bool secretTestMode=TEST_MODE_DEFAULT,editSecretTestMode=TEST_MODE_DEFAULT;
uint32_t lastGreetingDate=0,lastAlienChecked=0,feedCount=0;
bool dailyGreetingQueued=false, hungry=false, hungryFromSleep=false;
uint32_t hungerAskedAt=0;
uint32_t nextHungerAt=0;
uint8_t foodChoice=0;
uint32_t weatherDate=0;
int weatherTemp10=0,weatherWind10=0,weatherCode=-1;
bool weatherValid=false;
bool pendingAlien=false,pendingRoyal=false,pendingAngel=false,pendingFriend=false;



// ====================== CUTE TONE ENGINE ============================
// A passive piezo plays ORIGINAL little melodies with pitches near 220-520 Hz.
// No volume setting: Sound ON/OFF only. An active buzzer cannot play pitches.
// Non-blocking audio: no delay() inside sound playback.
struct PixoNote {uint16_t hz;uint16_t ms;};
const PixoNote STARTUP_TUNE[]={{262,175},{330,190},{392,200},{0,110},{440,230},{392,160},{523,360}};
const PixoNote WAKE_TUNE[]={{262,95},{330,105},{392,135},{523,230}};
const PixoNote SLEEP_TUNE[]={{330,190},{294,230},{262,280},{220,360}};
const PixoNote DREAM_TUNE[]={{220,90},{0,90},{247,120}};
const PixoNote BUTTON_A_TUNE[]={{277,45},{220,55}};
const PixoNote BUTTON_B_TUNE[]={{330,60},{392,70}};
const PixoNote PET_TUNE[]={{330,90},{392,110},{523,155}};
const PixoNote MUSIC_TUNE[]={{330,140},{0,65},{392,135},{440,125},{392,150},{523,180},{0,80},{440,250}};
const PixoNote FOOD_TUNE[]={{262,100},{330,100},{392,120},{523,180},{392,120},{523,245}};
const PixoNote HUNGRY_TUNE[]={{330,180},{0,120},{294,230},{0,80},{262,240}};
// Each secret has its own unmistakable original melody.
const PixoNote PEEK_TUNE[]={{392,120},{0,110},{392,110},{0,180},{523,100},{659,180},{784,230}};
const PixoNote ALIEN_TUNE[]={{220,130},{294,160},{247,145},{349,130},{233,160},{311,270},{0,90},{220,210}};
const PixoNote ANGEL_TUNE[]={{330,230},{392,230},{523,280},{0,110},{587,270},{659,360},{523,220}};
const PixoNote GHOST_TUNE[]={{294,330},{277,320},{247,310},{220,300},{0,200},{330,95},{0,70},{220,240}};
const PixoNote ROYAL_TUNE[]={{262,160},{392,160},{523,210},{0,110},{392,155},{523,155},{659,380}};
const PixoNote FRIEND_TUNE[]={{440,135},{523,135},{440,135},{392,130},{523,140},{659,245},{0,90},{523,220}};
const PixoNote BIRTHDAY_TUNE[]={{262,140},{262,140},{294,220},{262,220},{349,240},{330,360},{0,120},{392,170},{349,170},{330,180},{523,350}};
const PixoNote PREVIEW_TUNE[]={{330,100},{392,130}};
enum class SoundID:uint8_t {NONE, STARTUP, WAKE, SLEEP, DREAM, BUTTON_A, BUTTON_B,
  PET, MUSIC, SECRET, BIRTHDAY, PREVIEW, FOOD, HUNGRY};
SoundID activeSound=SoundID::NONE;
const PixoNote* activeNotes=nullptr;
uint8_t activeNoteCount=0,activeNoteIndex=0;
uint32_t activeNoteBegan=0,nextDreamChirp=0,nextSecretMelodyAt=0;
bool buzzerReady=false,forceSoundPreview=false;
void buzzerPwmSet(uint16_t frequency,uint8_t unused=0) {
  if(!buzzerReady)return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_PIN,frequency);
#else
  ledcWriteTone(BUZZER_LEDC_CHANNEL,frequency);
#endif
}
void buzzerHalt() {
  buzzerPwmSet(0);activeSound=SoundID::NONE;activeNotes=nullptr;
  activeNoteCount=0;forceSoundPreview=false;
}
void buzzerInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  buzzerReady=ledcAttach(BUZZER_PIN,BUZZER_FIXED_HZ,BUZZER_PWM_BITS);
#else
  ledcSetup(BUZZER_LEDC_CHANNEL,BUZZER_FIXED_HZ,BUZZER_PWM_BITS);
  ledcAttachPin(BUZZER_PIN,BUZZER_LEDC_CHANNEL);buzzerReady=true;
#endif
  buzzerPwmSet(0);
}
void buzzerPlay(SoundID id,uint32_t now,bool preview=false) {
  if(!buzzerReady || (!buzzerEnabled&&!preview) || (preview&&!editBuzzerEnabled))return;
  switch(id) {
    case SoundID::STARTUP: activeNotes=STARTUP_TUNE;activeNoteCount=sizeof(STARTUP_TUNE)/sizeof(PixoNote);break;
    case SoundID::WAKE: activeNotes=WAKE_TUNE;activeNoteCount=sizeof(WAKE_TUNE)/sizeof(PixoNote);break;
    case SoundID::SLEEP: activeNotes=SLEEP_TUNE;activeNoteCount=sizeof(SLEEP_TUNE)/sizeof(PixoNote);break;
    case SoundID::DREAM: activeNotes=DREAM_TUNE;activeNoteCount=sizeof(DREAM_TUNE)/sizeof(PixoNote);break;
    case SoundID::BUTTON_A: activeNotes=BUTTON_A_TUNE;activeNoteCount=sizeof(BUTTON_A_TUNE)/sizeof(PixoNote);break;
    case SoundID::BUTTON_B: activeNotes=BUTTON_B_TUNE;activeNoteCount=sizeof(BUTTON_B_TUNE)/sizeof(PixoNote);break;
    case SoundID::PET: activeNotes=PET_TUNE;activeNoteCount=sizeof(PET_TUNE)/sizeof(PixoNote);break;
    case SoundID::MUSIC: activeNotes=MUSIC_TUNE;activeNoteCount=sizeof(MUSIC_TUNE)/sizeof(PixoNote);break;
    case SoundID::BIRTHDAY: activeNotes=BIRTHDAY_TUNE;activeNoteCount=sizeof(BIRTHDAY_TUNE)/sizeof(PixoNote);break;
    case SoundID::FOOD: activeNotes=FOOD_TUNE;activeNoteCount=sizeof(FOOD_TUNE)/sizeof(PixoNote);break;
    case SoundID::HUNGRY: activeNotes=HUNGRY_TUNE;activeNoteCount=sizeof(HUNGRY_TUNE)/sizeof(PixoNote);break;
    case SoundID::SECRET:
      switch(secretAnim){
        case SecretAnim::PEEK: activeNotes=PEEK_TUNE;activeNoteCount=sizeof(PEEK_TUNE)/sizeof(PixoNote);break;
        case SecretAnim::ALIEN: activeNotes=ALIEN_TUNE;activeNoteCount=sizeof(ALIEN_TUNE)/sizeof(PixoNote);break;
        case SecretAnim::ANGEL: activeNotes=ANGEL_TUNE;activeNoteCount=sizeof(ANGEL_TUNE)/sizeof(PixoNote);break;
        case SecretAnim::GHOST: activeNotes=GHOST_TUNE;activeNoteCount=sizeof(GHOST_TUNE)/sizeof(PixoNote);break;
        case SecretAnim::ROYAL: activeNotes=ROYAL_TUNE;activeNoteCount=sizeof(ROYAL_TUNE)/sizeof(PixoNote);break;
        case SecretAnim::FRIEND: activeNotes=FRIEND_TUNE;activeNoteCount=sizeof(FRIEND_TUNE)/sizeof(PixoNote);break;
      }
      break;
    default: activeNotes=PREVIEW_TUNE;activeNoteCount=sizeof(PREVIEW_TUNE)/sizeof(PixoNote);break;
  }
  activeSound=id;activeNoteIndex=0;activeNoteBegan=now;forceSoundPreview=preview;
  buzzerPwmSet(activeNotes[0].hz);
}
void buzzerUpdate(uint32_t now) {
  if(activeSound==SoundID::NONE)return;
  if(!forceSoundPreview&&!buzzerEnabled){buzzerHalt();return;}
  // Catch up if an I2C frame took longer than one short musical note.
  while(activeSound!=SoundID::NONE &&
        now-activeNoteBegan>=activeNotes[activeNoteIndex].ms) {
    activeNoteBegan+=activeNotes[activeNoteIndex].ms;
    if(++activeNoteIndex>=activeNoteCount){buzzerHalt();return;}
    buzzerPwmSet(activeNotes[activeNoteIndex].hz);
  }
}

bool inSettingsDetail() {return state==State::MENU && menuDetail && menuIndex==6;}
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
  editBuzzerEnabled=buzzerEnabled;
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
  secretTestMode=petPrefs.getBool("testsec",TEST_MODE_DEFAULT);
  editSecretTestMode=secretTestMode;
  lastGreetingDate=petPrefs.getUInt("greetday",0);
  lastAlienChecked=petPrefs.getUInt("alienchk",0);
  royalMilestoneSaved=petPrefs.getUChar("royalm",0);
  feedCount=petPrefs.getUInt("feeds",0);
  weatherDate=petPrefs.getUInt("wxdate",0);
  weatherTemp10=petPrefs.getInt("wxtemp",0);
  weatherWind10=petPrefs.getInt("wxwind",0);
  weatherCode=petPrefs.getInt("wxcode",-1);
  weatherValid=weatherDate!=0 && weatherCode>=0;
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
    pet.secrets |= (1U<<SEC_BIRTHDAY); // birthday badge
    markPetDirty();
    savePetIfNeeded(millis(),true);
  }
}


void checkSecretMilestones();
void checkPetDateDuringRuntime(uint32_t now) {
  // Catch a birthday at midnight even when PIXO has stayed powered on.
  if(!clockSynced || now-lastValidDateCheck < 60000) return;
  lastValidDateCheck=now;
  uint32_t today=currentLocalDate();
  checkSecretMilestones();
  if(isAnniversary(today) && pet.lastBirthdayShownYmd!=today) {
    anniversaryQueued=true;
    pet.lastBirthdayShownYmd=today;
    pet.secrets |= (1U<<SEC_BIRTHDAY);
    markPetDirty();
    savePetIfNeeded(now,true);
  }
}

bool isMidnightWindow() {
 if(!clockSynced)return false;
 time_t raw=time(nullptr);tm local;
 if(!localtime_r(&raw,&local))return false;
 return local.tm_hour==0 && local.tm_min<15;
}
// Secret days are based on the original birth date, not time powered on.
void checkSecretMilestones() {
 if(!clockSynced||!pet.birthYmd)return;
 uint32_t age=petAgeDays();
 const uint16_t royalDays[]={7,14,30,90,180,270};
 uint8_t reachedCount=0;
 for(uint8_t i=0;i<6;i++)if(age>=royalDays[i])reachedCount=i+1;
 if(reachedCount>royalMilestoneSaved) {
   royalMilestoneSaved=reachedCount;petPrefs.putUChar("royalm",royalMilestoneSaved);
   pet.secrets|=(1U<<SEC_ROYAL);markPetDirty();pendingRoyal=true;
 }
 if(age>=365) {
   if(!(pet.secrets&(1U<<SEC_ANGEL))) {
     pet.secrets|=(1U<<SEC_ANGEL);markPetDirty();pendingAngel=true;
   }
   if(!(pet.secrets&(1U<<SEC_FRIEND))) {
     pet.secrets|=(1U<<SEC_FRIEND);markPetDirty();pendingFriend=true;
   }
 }
 uint32_t today=currentLocalDate();
 if(age>=21 && today && today!=lastAlienChecked && !(pet.secrets&(1U<<SEC_ALIEN))) {
   lastAlienChecked=today;petPrefs.putUInt("alienchk",today);
   if(random(22)==0){pet.secrets|=(1U<<SEC_ALIEN);markPetDirty();pendingAlien=true;}
 }
}
void checkDailyGreeting() {
 if(!clockSynced)return;
 uint32_t today=currentLocalDate();
 if(today && today!=lastGreetingDate)dailyGreetingQueued=true;
}
void markDailyGreetingShown() {
 uint32_t today=currentLocalDate();
 if(today){lastGreetingDate=today;petPrefs.putUInt("greetday",today);}
 dailyGreetingQueued=false;
}
const char* weatherLabel(int c) {
 if(c<0)return "NO WEATHER";
 if(c==0)return "CLEAR SKIES";
 if(c<=3)return "PARTLY CLOUDY";
 if(c==45||c==48)return "MISTY";
 if(c>=51&&c<=67)return "RAINY DAY";
 if(c>=71&&c<=77)return "SNOW";
 if(c>=80&&c<=82)return "SHOWERS";
 if(c>=95)return "THUNDER";
 return "CLOUDY";
}
void setState(State next,uint32_t now);
void startSecretAnimation(SecretAnim which,uint32_t now,bool unlock=true) {
 secretAnim=which;
 // Only genuine unlocks set permanent flags; test preview doesn't age PIXO.
 if(unlock) {
   switch(which){
     case SecretAnim::PEEK:pet.secrets|=(1U<<SEC_PEEK);break;
     case SecretAnim::GHOST:pet.secrets|=(1U<<SEC_GHOST);break;
     default:break;
   }
   markPetDirty();savePetIfNeeded(now,true);
 }
 setState(State::SECRET_REACTION,now);
 nextSecretMelodyAt=now+3800;
}
void startFeeding(uint32_t now) {
 hungry=false;foodChoice=(uint8_t)random(3);
 nextHungerAt=now+HUNGER_INTERVAL_MS;
 hungryFromSleep=false;lastInteraction=now;
 if(feedCount<UINT32_MAX)++feedCount;
 petPrefs.putUInt("feeds",feedCount);
 pet.affection=(uint16_t)min(100,(int)pet.affection+3);
 markPetDirty();notePlayedToday();savePetIfNeeded(now,true);
 setState(State::FEEDING,now);
 buzzerPlay(SoundID::FOOD,now);
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
  // A complete little birthday party, paced over 14.6 seconds.
  uint32_t scene=(dt/3500UL)%4;
  float bounce=3*fabsf(sinf(dt*.007f));
  oneEye(34,35-(int)bounce,.85f,-1);
  oneEye(94,35-(int)bounce,.85f,1);
  display.setTextSize(1);
  display.setCursor(16,2);display.print(F("HAPPY PIXO DAY!"));
  int cakeY=scene==0 ? 60-(int)(26*ease(progress(dt,1500))) : 34;
  // Decorated two-layer cake with frosting, candle and animated flame.
  display.drawRoundRect(51,cakeY+9,27,15,3,SSD1306_WHITE);
  display.fillRect(53,cakeY+20,23,3,SSD1306_WHITE);
  display.drawFastHLine(51,cakeY+14,27,SSD1306_WHITE);
  for(int i=0;i<4;i++)display.drawPixel(55+i*6,cakeY+16,SSD1306_WHITE);
  display.drawFastVLine(64,cakeY+3,7,SSD1306_WHITE);
  if(dt<6500)display.fillCircle(64,cakeY+1+(int)sinf(dt*.014f),2,SSD1306_WHITE);
  else if(dt<8200){display.drawCircle(65+(int)((dt-6500)/150),cakeY-1-(int)((dt-6500)/320),2,SSD1306_WHITE);}
  if(dt>7100)for(int i=0;i<14;i++){
    int x=(i*37+9+(int)(dt/30))%128;
    int y=(int)((dt/17+i*17)%64);
    if(i%2)display.drawLine(x,y,x+2,y,SSD1306_WHITE);
    else tinySparkle(x,y,2);
  }
  display.fillRect(0,55,128,9,SSD1306_BLACK);
  display.setCursor(scene==0?31:scene==1?16:scene==2?25:17,56);
  switch(scene) {
    case 0: display.print(F("MAKE A WISH!"));break;
    case 1: display.print(F("BLOW THE CANDLE!"));break;
    case 2: display.print(F("YAY! CONFETTI!"));break;
    default: display.print(F("LOVE YOU LOTS!"));break;
  }
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
  display.setCursor(20,4);display.print(F("Hii! I'm your"));
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
  display.setCursor(8,6);display.print(F("PIXO / OUR STORY"));
  display.drawFastHLine(10,17,108,SSD1306_WHITE);
  char d[18];fmtDate(d,sizeof(d),pet.birthYmd);
  display.setCursor(11,24);display.print(F("B-day: "));display.print(d);
  display.setCursor(11,36);display.print(F("I'm "));
  if(clockSynced && pet.birthYmd) {
    display.print((unsigned long)petAgeDays());display.print(F(" days!"));
  } else display.print(F("Waiting for time"));
  display.setCursor(11,49);display.print(F("My human: "));
  display.print(savedOwner.substring(0,8));
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
  display.setCursor(10,5);display.print(F("BANGALORE / TODAY"));
  time_t raw=time(nullptr);
  if (!clockSynced || raw < (time_t)1700000000) {
    display.setCursor(13,23);display.print(F("CLOCK IS SLEEPY!"));
    display.setCursor(13,39);display.print(F("WiFi will fix it!"));
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

void uiHeader(const char*,uint8_t,const char*);
void uiLabel(int,int,const char*);
void uiValue(int,int,const String&);
void drawWeather(uint32_t now) {
 uiHeader("BANGALORE",7,"TODAY");
 uint32_t today=clockSynced?currentLocalDate():0;
 if(!weatherValid||weatherDate!=today) {
   uiLabel(8,24,"CLOUDS ARE SHY!");
   uiLabel(8,39,"WiFi next time <3");
 } else {
   char temp[20];snprintf(temp,sizeof(temp),"%d.%d C",weatherTemp10/10,abs(weatherTemp10%10));
   uiValue(9,20,temp);
   uiLabel(9,42,weatherLabel(weatherCode));
   char wind[24];snprintf(wind,sizeof(wind),"WIND %d km/h",weatherWind10/10);
   uiLabel(9,53,wind);
 }
}
void drawDailyGreeting(uint32_t dt) {
 uiHeader("HI, MY HUMAN!",0,"<3");
 time_t raw=time(nullptr);tm t;
 bool valid=clockSynced&&localtime_r(&raw,&t);
 int hour=valid?t.tm_hour:12;
 const char* hello=hour<12?"MORNING, SUNBEAM!":hour<17?"HEYA, SUNSHINE!":hour<21?"COZY EVENING!":"SWEET DREAM TIME!";
 uiLabel(7,18,hello);
 display.setTextSize(1);
 display.setCursor(7,29);display.print(F("Hi "));display.print(savedOwner.substring(0,13));display.print(F("!"));
 const char* msg[]={"My lucky star <3","Tiny hugs for you!","Let's smile!","Today is our day <3","You make me happy!","Big love, tiny PIXO!"};
 uint32_t day=currentLocalDate();
 uiLabel(7,44,msg[day%6]);
 if((dt/220)%2)tinyHeart(113,43,4);
 display.drawFastHLine(7,57,110,SSD1306_WHITE);
 display.fillRect(7,57,(int)(110*progress(dt,GREETING_SHOW_MS)),2,SSD1306_WHITE);
}
void drawPeekaboo(uint32_t dt) {
  // Three complete hand-cover / surprise / giggle cycles.
  uint32_t cycle=dt%4000UL;
  if(cycle<1550){
    eyes(.88f,0,0,0);
    for(int i=0;i<2;++i){
      int x=i?RIGHT_EYE_X:LEFT_EYE_X;
      int down=(int)(12*ease(progress(cycle,390)));
      display.fillRoundRect(x-18,5+down,36,28,7,SSD1306_WHITE);
      for(int f=0;f<4;f++)display.drawLine(x-14+f*8,7+down,x-14+f*8,27+down,SSD1306_BLACK);
    }
    uiLabel(43,55,"SHHH... <3");
  } else {
    uint32_t popped=cycle-1550;
    float pop=ease(progress(popped,240));
    eyes(.46f+.54f*pop,0,0,(int)(-2*sinf(popped*.012f)),pop);
    tinySparkle(12,12,4);tinySparkle(116,16,4);
    if((cycle/200)%2) {tinyHeart(64,7,4);tinySparkle(63,26,3);}
    uiLabel(34,55,popped<1000?"PEEKABOO!":"HEHE~ GOTCHA!");
  }
}
void drawSecretEvent(uint32_t dt) {
 // Entire 12.55-second Easter egg is animated, not a held last frame.
 switch(secretAnim){
   case SecretAnim::PEEK: drawPeekaboo(dt);break;
   case SecretAnim::ALIEN: {
     int ufoX=dt<2200?mixInt(-18,64,ease(progress(dt,2200))):
              (dt>10000?mixInt(64,155,ease(progress(dt-10000,2400))):64);
     int xo=LEFT_EYE_X,yo=32;
     bool abducting=dt>=2500&&dt<7600;
     if(abducting){
       int lift=dt<5200?(int)(23*ease(progress(dt-2500,2700))):
                (int)(23*(1-ease(progress(dt-5200,2400))));
       yo-=lift;xo+=(int)(15*sinf(dt*.012f));
     }
     if(abducting){oneEye(RIGHT_EYE_X,32,.9f,1);oneEye(xo,yo,.9f,-1);}
     else eyes(.92f,0,0,0);
     display.drawRoundRect(ufoX-16,3,32,9,4,SSD1306_WHITE);
     display.drawFastHLine(ufoX-21,13,42,SSD1306_WHITE);
     display.drawCircle(ufoX,3,4,SSD1306_WHITE);
     if(abducting){display.drawLine(ufoX,15,xo,yo-12,SSD1306_WHITE);}
     if(dt>7600 && dt<9700){
       display.drawCircle(LEFT_EYE_X,32,20,SSD1306_WHITE);
       display.drawFastVLine(LEFT_EYE_X,20,24,SSD1306_BLACK);
       tinySparkle(LEFT_EYE_X,10,3);
     }
     uiLabel(22,55,dt<2500?"HI, SPACE BUDDY!":dt<7600?"HEY! MY EYE!":dt<10000?"UPSIDE DOWN?!":"PHEW! I'M BACK!");
     break;
   }
   case SecretAnim::ANGEL: {
     eyes(.84f,0,0,(int)(2*sinf(dt*.005f)));
     int flap=(int)(8*fabsf(sinf(dt*.006f)));
     display.drawRoundRect(48,2+(int)(2*sinf(dt*.005f)),33,6,3,SSD1306_WHITE);
     display.drawLine(22,30,5,9+flap,SSD1306_WHITE);
     display.drawLine(5,9+flap,13,43,SSD1306_WHITE);
     display.drawLine(13,43,22,30,SSD1306_WHITE);
     display.drawLine(106,30,123,9+flap,SSD1306_WHITE);
     display.drawLine(123,9+flap,115,43,SSD1306_WHITE);
     display.drawLine(115,43,106,30,SSD1306_WHITE);
     display.drawLine(21,31,8,23+flap,SSD1306_WHITE);
     display.drawLine(107,31,120,23+flap,SSD1306_WHITE);
     if((dt/220)%2==0){tinySparkle(15,48,4);tinySparkle(112,47,3);}
     uiLabel(24,55,dt<5000?"LOOK! TINY WINGS!":"FLY WITH ME~ <3");break;
   }
   case SecretAnim::GHOST: {
     int x=(int)(-25+((dt*177UL)/SECRET_CELEBRATE_MS));
     bool hiding=dt>=2400&&dt<7900;
     eyes(hiding?.18f:.85f,0,0,hiding?11:0);
     display.fillRoundRect(x,5+(int)(4*sinf(dt*.004f)),16,19,7,SSD1306_WHITE);
     display.drawPixel(x+5,15,SSD1306_BLACK);display.drawPixel(x+11,15,SSD1306_BLACK);
     if(dt>7800)tinySparkle(110,13,4);
     uiLabel(25,55,hiding?"IS IT GONE...?":"BOO! JUST KIDDING!");break;
   }
   case SecretAnim::ROYAL: {
     drawStarry(dt);
     int y=dt<2400?mixInt(-16,3,ease(progress(dt,2400))):
       3+(int)(2*sinf(dt*.006f));
     display.fillTriangle(48,y+9,53,y,59,y+9,SSD1306_WHITE);
     display.fillTriangle(58,y+9,64,y-2,70,y+9,SSD1306_WHITE);
     display.fillTriangle(69,y+9,75,y,80,y+9,SSD1306_WHITE);
     display.fillRect(48,y+9,32,4,SSD1306_WHITE);
     if(dt>2500 && (dt/180)%2){tinySparkle(14,18,4);tinySparkle(115,19,4);}
     display.fillRect(0,54,128,10,SSD1306_BLACK);
     uiLabel(26,55,dt<2800?"YOUR MAJESTY!":"ROYAL PIXO <3");break;
   }
   case SecretAnim::FRIEND: {
     eyes(.9f,0,0,(int)(2*sinf(dt*.005f)));
     float t=ease(progress(dt,2600));
     int x=mixInt(142,108,t)+(int)(10*sinf(dt*.007f));
     int y=13+(int)(8*sinf(dt*.008f));
     display.fillCircle(x,y,5,SSD1306_WHITE);
     display.drawCircle(x-5,y-5,3,SSD1306_WHITE);
     display.drawCircle(x+5,y-5,3,SSD1306_WHITE);
     display.drawFastHLine(x-3,y,6,SSD1306_BLACK);
     if(dt>3400){tinyHeart(61,8+(int)(3*sinf(dt*.005f)),4);tinySparkle(92,15,3);}
     uiLabel(23,55,dt<3200?"WHO'S THAT?!":"MEET MY NEW BESTIE!");break;
   }
 }
}
void drawBeeFriend(uint32_t now) {
 int y=6+(int)(2*sinf(now*.005f));
 display.fillCircle(116,y,4,SSD1306_WHITE);
 display.drawCircle(111,y-5,3,SSD1306_WHITE);
 display.drawCircle(120,y-5,3,SSD1306_WHITE);
 display.drawPixel(115,y,SSD1306_BLACK);
}
void drawCuteStrawberry(int x,int y,uint32_t dt) {
  // Plump red-berry silhouette in monochrome with true leafy top and seeds.
  display.fillCircle(x-7,y-2,7,SSD1306_WHITE);
  display.fillCircle(x+7,y-2,7,SSD1306_WHITE);
  display.fillTriangle(x-13,y,x,y+20,x+13,y,SSD1306_WHITE);
  display.fillTriangle(x-10,y-11,x-3,y-3,x-1,y-11,SSD1306_WHITE);
  display.fillTriangle(x+1,y-12,x+3,y-3,x+11,y-11,SSD1306_WHITE);
  display.drawFastVLine(x,y-15,6,SSD1306_WHITE);
  for(int i=0;i<6;i++) {
    int sx=x-8+(i%3)*8,sy=y+1+(i/3)*7;
    display.drawPixel(sx,sy,SSD1306_BLACK);
    display.drawPixel(sx+1,sy+1,SSD1306_BLACK);
  }
  if((dt/220)%2)tinySparkle(x+17,y-8,2);
}
void drawCuteJuice(int x,int y,uint32_t dt) {
  display.drawRoundRect(x-12,y-7,24,28,4,SSD1306_WHITE);
  display.drawFastHLine(x-10,y+1,20,SSD1306_WHITE);
  display.fillRect(x-10,y+3,20,15,SSD1306_WHITE);
  display.drawLine(x+5,y-7,x+11,y-17,SSD1306_WHITE);
  display.drawLine(x+11,y-17,x+15,y-17,SSD1306_WHITE);
  display.drawFastHLine(x-14,y-9,28,SSD1306_WHITE);
  display.drawCircle(x-3,y+9,2,SSD1306_BLACK);
  display.drawCircle(x+5,y+14,2,SSD1306_BLACK);
  display.drawCircle(x+16,y+1-(int)(dt/500)%7,2,SSD1306_WHITE);
}
void drawCuteCake(int x,int y,uint32_t dt) {
  display.fillRoundRect(x-16,y-2,32,18,3,SSD1306_WHITE);
  display.fillRect(x-16,y+5,32,5,SSD1306_BLACK);
  display.drawFastHLine(x-18,y-5,36,SSD1306_WHITE);
  for(int i=0;i<4;i++)display.fillCircle(x-13+i*9,y-5,3,SSD1306_WHITE);
  display.drawFastVLine(x,y-12,7,SSD1306_WHITE);
  display.fillCircle(x,y-15+(int)sinf(dt*.016f),2,SSD1306_WHITE);
  display.drawPixel(x-7,y+13,SSD1306_BLACK);
  display.drawPixel(x+7,y+13,SSD1306_BLACK);
}
void drawFeeding(uint32_t dt) {
  // Four acts: reveal, fly up, cute bites, gratitude.
  display.setTextSize(1);
  display.setCursor(4,1);
  display.print(dt<1200?F("OH! FOR ME? <3"):dt<2800?F("NOM NOM NOM!"):F("YUMMY! THANK U!"));
  int bob=(int)(3*fabsf(sinf(dt*.012f)));
  eyes(.80f+(dt>2800?.13f:0),0,0,bob-1);
  drawBlush();
  int fx=64+(int)(7*sinf(dt*.005f));
  int fy=dt<1200?mixInt(60,37,ease(progress(dt,1200))):37;
  bool eaten=dt>2650;
  if(!eaten){
    if(foodChoice==0)drawCuteStrawberry(fx,fy,dt);
    else if(foodChoice==1)drawCuteJuice(fx,fy,dt);
    else drawCuteCake(fx,fy,dt);
    if(dt>1500){
      // Bitten-food visual, not an added mouth on PIXO's face.
      display.fillCircle(fx+12,fy+9,4,SSD1306_BLACK);
      display.fillCircle(fx+10,fy+5,3,SSD1306_BLACK);
    }
  } else {
    tinyHeart(64,25-(int)(4*sinf(dt*.008f)),5);
    tinySparkle(28,30,4);tinySparkle(100,27,4);
  }
  display.fillRect(0,55,128,9,SSD1306_BLACK);
  display.setTextSize(1);display.setCursor(18,56);
  if(dt<2500)display.print(foodChoice==0?F("SWEET BERRY!"):foodChoice==1?F("SIPPY JUICE!"):F("YUMMY CAKE!"));
  else {display.print(F("LOVE YOU, "));display.print(savedOwner.substring(0,7));}
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
    uiLabel(5,17,"MY LITTLE LIFE <3");
    if(clockSynced && pet.birthYmd) {
      snprintf(tmp,sizeof(tmp),"%lu days",(unsigned long)petAgeDays());
      uiValue(5,28,tmp);
    } else uiValue(5,28,"No time yet");
    char dob[18];fmtDate(dob,sizeof(dob),pet.birthYmd);
    uiLabel(5,45,"BORN");uiLabel(36,45,dob);
  } else if(menuPage==1) {
    uiLabel(5,17,"ALL MY LOVE FOR YOU");
    snprintf(tmp,sizeof(tmp),"%u / 100",(unsigned)pet.affection);
    uiValue(5,28,tmp);
    uiMiniBar(5,46,113,(uint8_t)pet.affection,100);
    // Total pets available on next card as a large number.
  } else if(menuPage==2) {
    uiLabel(5,17,"TOTAL LITTLE PETS");
    snprintf(tmp,sizeof(tmp),"%lu",(unsigned long)pet.totalPets);uiValue(5,28,tmp);
    uiLabel(5,45,"MOOD");uiLabel(43,45,PERSONALITIES[pet.personality%5]);
  } else if(menuPage==3) {
    uiLabel(5,17,"GROWING UP SO FAST!");
    uiValue(5,28,growthStage());
    uiLabel(5,45,"WEAR:");uiLabel(42,45,ACCESSORIES[pet.accessory]);
  } else if(menuPage==4) {
    uiLabel(5,17,"OUR DAYS / TREASURES");
    snprintf(tmp,sizeof(tmp),"%u days",(unsigned)pet.dayStreak);uiValue(5,28,tmp);
    snprintf(tmp,sizeof(tmp),"UNLOCKED EASTER EGGS: %u",(unsigned)unlockedCount());
    uiLabel(5,45,tmp);
  } else if(menuPage==5) {
    uiLabel(5,17,"LAST PLAY DAY <3");
    char last[18];fmtDate(last,sizeof(last),pet.lastPlayedYmd);
    uiValue(5,28,last);
    uiLabel(5,45,"Miss you already!");
  } else {
    uiLabel(5,17,"MY FAVE HUMAN");
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
    uiLabel(6,17,"HOW BRIGHT AM I?");
    uiValue(6,28,BRIGHTNESS_LABELS[editBrightnessLevel]);
    uiMiniBar(6,46,115,editBrightnessLevel+1,BRIGHTNESS_LEVEL_COUNT);
    uiFooter("A:BACK","B:NEXT","H:SAVE");
  } else if(settingsView==SettingsView::AUTO_NIGHT) {
    uiHeader("AUTO NIGHT",7,"22-7");
    uiLabel(6,17,"COZY LIGHTS AT 10!");
    uiValue(8,28,editAutoNightMode ? "ON" : "OFF");
    uiLabel(6,45,clockSynced?"I know bedtime <3":"Need WiFi clock <3");
    uiFooter("A:BACK","B:TOGG","H:SAVE");
  } else if(settingsView==SettingsView::BUZZER_ENABLE) {
    uiHeader("PIXO SOUNDS",5,"AUDIO");
    uiLabel(6,17,"MY TINY JINGLES  <3");
    uiValue(8,29,editBuzzerEnabled ? "ON" : "OFF");
    uiLabel(6,46,"B:TOGGLE  HOLD:SAVE");
    uiFooter("A:BACK","B:TOGG","H:SAVE");
  } else if(settingsView==SettingsView::TEST_SECRETS) {
    uiHeader("TEST SECRETS",3,"DEBUG");
    uiLabel(7,17,"SNEAK PEEK AT MAGIC!");
    uiValue(8,29,editSecretTestMode?"ON":"OFF");
    uiLabel(7,45,"Just pretend! No prizes");
    uiFooter("A:BACK","B:TOGG","H:SAVE");

  } else if(settingsView==SettingsView::CLOCK) {
    uiHeader("CLOCK SYNC",1);
    uiLabel(7,19,clockSynced?"YAY! CLOCK IS HAPPY":"CLOCK IS SLEEPY!");
    uiLabel(7,32,"WiFi is napping");
    uiLabel(7,45,"Asia/Kolkata IST");
    uiFooter("A:BACK");
  } else if(settingsView==SettingsView::OWNER) {
    uiHeader("MY FAVE HUMAN",0);
    uiLabel(7,18,"MY FAVE HUMAN <3");
    uiValue(7,28,savedOwner);
    uiLabel(7,48,"Edit in PIXO code");
    uiFooter("A:BACK");
  } else if(settingsView==SettingsView::MEMORY) {
    uiHeader("PET MEMORY",3);
    uiLabel(7,20,"OUR MEMORIES LIVE HERE");
    uiLabel(7,34,"Safe in my tiny brain!");
    uiLabel(7,45,"Our story stays <3");
    uiFooter("A:BACK");
  } else if(settingsView==SettingsView::RESET) {
    uiHeader("FACTORY RESET",8);
    if(!resetArmed) {
      uiLabel(7,19,"START OUR STORY OVER?");
      uiLabel(7,32,"Bye, birthdays & gifts!");
      uiLabel(7,45,"Tap B if you're sure");
      uiFooter("A:BACK","B:ARM");
    } else {
      uiLabel(7,17,"REALLY SAY GOODBYE?");
      uiLabel(7,30,"Hold B 3s to restart");
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
  if((now/2200)%2==0){
    display.print(ACCESSORIES[wardrobePreview]);
    char item[7];snprintf(item,sizeof(item),"%u/%u",(unsigned)(wardrobePreview+1),(unsigned)ACCESSORY_COUNT);
    display.setCursor(99,56);display.print(item);
  } else display.print(F("B:NEXT  HOLD:SAVE"));
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
      "Our tiny story", "Pretty little day", "A surprise someday",
      "My secret treasures", "Dress me up!", "Clouds and sunshine", "Tiny tweaks for me"
    };
    uiCardItem(MENU_ITEMS[menuIndex],subtitles[menuIndex],menuIndex,menuIndex,MENU_ITEMS_COUNT);
    return;
  }
  if(menuIndex==0) drawMyPixoDetail();
  else if(menuIndex==1) drawCalendar(now);
  else if(menuIndex==2) {
    uiHeader("BATTERY",2);
    uiIcon(2,54,20,SSD1306_WHITE);
    uiLabel(27,42,"Soon, pinky promise!");uiFooter("A:BACK");
  } else if(menuIndex==3) {
    const uint8_t idx=secretPage%SECRET_COUNT;
    char page[12];snprintf(page,sizeof(page),"%u/%u",idx+1,SECRET_COUNT);
    uiHeader("SECRET DIARY",3,page);
    display.setTextSize(1);
    display.setCursor(5,17);display.print(SECRET_NAMES[idx]);
    display.drawFastHLine(5,27,118,SSD1306_WHITE);
    uiLabel(5,29,SECRET_HINTS[idx][0]);
    uiLabel(5,38,SECRET_HINTS[idx][1]);
    uiLabel(5,46,SECRET_HINTS[idx][2]);
    // Three crisp hint lines above the button footer.
    uiFooter("A:BACK",(pet.secrets&(1U<<idx))?"GOT IT!":"HIDDEN",secretTestMode?"H:TRY":"B:NEXT");
  } else if(menuIndex==4) drawWardrobe(now);
  else if(menuIndex==5)drawWeather(now);
  else if(menuIndex==6)drawSettingsPage(now);
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
  else if(menuIndex==3)secretPage=(secretPage+1)%SECRET_COUNT;
  else if(menuIndex==4) wardrobePreview=(wardrobePreview+1)%ACCESSORY_COUNT;
  else if(menuIndex==6) {
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
      case SettingsView::TEST_SECRETS:editSecretTestMode=!editSecretTestMode;break;
      case SettingsView::RESET: resetArmed=!resetArmed;break;
      default:break;
    }
  }
}
void menuSelect(uint32_t now) {
  lastInteraction=now;
  if(!menuDetail) {
    if(menuIndex==1){setState(State::CALENDAR,now);return;}
    if(menuIndex==5){setState(State::WEATHER,now);return;}
    menuDetail=true;menuPage=0;settingsView=SettingsView::LIST;
    if(menuIndex==4)wardrobePreview=pet.accessory;
    if(menuIndex==3)secretPage=0;
  } else if(menuIndex==3 && secretTestMode) {
    switch(secretPage){
      case SEC_PEEK:startSecretAnimation(SecretAnim::PEEK,now,false);break;
      case SEC_BIRTHDAY:setState(State::INTRO_BIRTHDAY,now);break;
      case SEC_ALIEN:startSecretAnimation(SecretAnim::ALIEN,now,false);break;
      case SEC_ANGEL:startSecretAnimation(SecretAnim::ANGEL,now,false);break;
      case SEC_GHOST:startSecretAnimation(SecretAnim::GHOST,now,false);break;
      case SEC_ROYAL:startSecretAnimation(SecretAnim::ROYAL,now,false);break;
      case SEC_FRIEND:startSecretAnimation(SecretAnim::FRIEND,now,false);break;
    }
  } else if(menuIndex==4) {
    pet.accessory=wardrobePreview;
    markPetDirty();savePetIfNeeded(now,true);
    menuFeedbackUntil=now+650;
  } else if(menuIndex==6) {
    if(settingsView==SettingsView::LIST) {
      settingsView=(SettingsView)(settingsIndex+1);
      if(settingsView==SettingsView::BRIGHTNESS)editBrightnessLevel=savedBrightnessLevel;
      if(settingsView==SettingsView::AUTO_NIGHT)editAutoNightMode=autoNightMode;
      if(settingsView==SettingsView::BUZZER_ENABLE)editBuzzerEnabled=buzzerEnabled;
      if(settingsView==SettingsView::TEST_SECRETS)editSecretTestMode=secretTestMode;
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
    } else if(settingsView==SettingsView::TEST_SECRETS) {
      secretTestMode=editSecretTestMode;
      petPrefs.putBool("testsec",secretTestMode);
      settingsView=SettingsView::LIST;
    }
  }
}
void menuBack(uint32_t now) {
  lastInteraction=now;
  if(state!=State::MENU) {showMenu(now);return;}
  if(!menuDetail) {setState(State::NORMAL,now);scheduleExpressions(now);return;}
  if(menuIndex==6 && settingsView!=SettingsView::LIST) {
    // Cancel changes without writing them to flash.
    settingsView=SettingsView::LIST;
    editBrightnessLevel=savedBrightnessLevel;
    editAutoNightMode=autoNightMode;
    editBuzzerEnabled=buzzerEnabled;
    editSecretTestMode=secretTestMode;

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
    st==State::INTRO_PROFILE || st==State::INTRO_BIRTHDAY ||
    st==State::DAILY_GREETING;
}
void buttonShort(ButtonReader &b,uint32_t now) {
  if(isIntroState(state)) return;
  notePlayedToday();
  if(state==State::SLEEP || state==State::DROWSY) {
    lastInteraction=now;petAfterWake=false;setState(State::WAKING,now);
    return;  // One press wakes, following presses use the menu / feed.
  }
  const bool soundEditor=inSettingsDetail() && settingsView==SettingsView::BUZZER_ENABLE;
  if(b.pin==BUTTON_A_PIN) menuBack(now);
  else if(state==State::MENU) menuNext(now);
  else if(b.pin==BUTTON_B_PIN && hungry)startFeeding(now);
  if(!soundEditor && state!=State::FEEDING) buzzerPlay(b.pin==BUTTON_A_PIN?SoundID::BUTTON_A:SoundID::BUTTON_B,now);
}
void buttonLong(ButtonReader &b,uint32_t now) {
  if(isIntroState(state)) return;
  notePlayedToday();
  if(state==State::SLEEP || state==State::DROWSY) {
    lastInteraction=now;petAfterWake=false;setState(State::WAKING,now);
    return;
  }
  if(b.pin==BUTTON_A_PIN) menuHome(now);
  else if(state==State::MENU) menuSelect(now);
  if(buzzerEnabled)
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
      notePlayedToday();
      lastInteraction=now;
      startSecretAnimation(isMidnightWindow()?SecretAnim::GHOST:SecretAnim::PEEK,now);
      Serial.println(F("PIXO: Peekaboo / midnight ghost button chord"));
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

// ======================== BENGALURU WEATHER ========================
// Open-Meteo public non-commercial API; HTTPS is used, certificate check
// is disabled on prototype firmware (public non-sensitive weather only).
// For production, install CA root pinning/cert validation.
// Parse only current weather block; no ArduinoJson dependency.
bool parseWxNumber(const String& block,const char* key,float &value) {
 String needle=String("\"")+key+"\"";
 int at=block.indexOf(needle);
 if(at<0)return false;
 int colon=block.indexOf(':',at+needle.length());if(colon<0)return false;
 const char* p=block.c_str()+colon+1;
 char* end=nullptr;
 value=strtof(p,&end);
 return end!=p;
}
void fetchWeatherAtStartup() {
 if(!WEATHER_ENABLED || WiFi.status()!=WL_CONNECTED)return;
 WiFiClientSecure client;client.setInsecure();
 HTTPClient http;
 const char* url="https://api.open-meteo.com/v1/forecast?latitude=12.9716&longitude=77.5946&current=temperature_2m,weather_code,wind_speed_10m&timezone=Asia%2FKolkata";
 http.setConnectTimeout(2500);
 http.setTimeout(3500);
 if(!http.begin(client,url)){Serial.println(F("PIXO: weather URL failed"));return;}
 int status=http.GET();
 if(status==HTTP_CODE_OK) {
   String payload=http.getString();
   int at=payload.indexOf("\"current\"");
   if(at>=0){
     String block=payload.substring(at);
     float temperature=0,code=0,wind=0;
     if(parseWxNumber(block,"temperature_2m",temperature) &&
        parseWxNumber(block,"weather_code",code) &&
        parseWxNumber(block,"wind_speed_10m",wind)) {
       weatherTemp10=(int)roundf(temperature*10.0f);
       weatherCode=(int)code;
       weatherWind10=(int)roundf(wind*10.0f);
       weatherDate=currentLocalDate();
       weatherValid=weatherDate!=0;
       if(weatherValid){
         petPrefs.putInt("wxtemp",weatherTemp10);
         petPrefs.putInt("wxcode",weatherCode);
         petPrefs.putInt("wxwind",weatherWind10);
         petPrefs.putUInt("wxdate",weatherDate);
       }
       Serial.println(F("PIXO: weather updated"));
     }
   }
 } else Serial.printf("PIXO weather HTTP: %d\n",status);
 http.end();
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
  if(clockSynced) {
    onClockReady();checkDailyGreeting();checkSecretMilestones();
  }
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
      configTzTime(TIMEZONE_POSIX,NTP_SERVER_1,NTP_SERVER_2);
      wifiStage=WifiStage::SYNCING;
      wifiStageStarted=now;
      Serial.println(F("PIXO: WiFi connected; synchronizing date/time"));
    } else if (now-wifiStageStarted>=WIFI_CONNECT_TIMEOUT_MS) {
      completeIntroWiFi(WifiStage::CONNECT_FAILED,now);
    }
  } else if (wifiStage==WifiStage::SYNCING) {
    if (time(nullptr)>(time_t)1700000000) {
      fetchWeatherAtStartup();
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
  display.setTextSize(1);
  display.setCursor(49,16);display.print(F("ARUN"));
  display.setCursor(43,28);display.print(F("YOKESH"));
  display.setTextSize(1);
  display.setCursor(12,45);
  switch(wifiStage) {
    case WifiStage::CONNECTING: display.print(F("Finding WiFi... <3"));break;
    case WifiStage::SYNCING:
      if (now-wifiStageStarted<500) display.print(F("WiFi connected!"));
      else display.print(F("Finding today..."));
      break;
    case WifiStage::OK: display.print(F("Yay! I know today!"));break;
    case WifiStage::NO_CONFIG: display.print(F("Where is my WiFi?"));break;
    case WifiStage::CONNECT_FAILED: display.print(F("Oops! No WiFi :(") );break;
    case WifiStage::NTP_FAILED: display.print(F("Clock is shy today"));break;
    default: display.print(F("Wake up, WiFi!"));break;
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
  // Slow breathing, blinking closed lids, dream bubbles and gentle Zzz.
  float breath=sinf(((float)(dt%4200UL)/4200.0f)*6.2831853f);
  int by=(int)(2*breath);
  for (int i=0;i<2;i++) {
    int x=i?RIGHT_EYE_X:LEFT_EYE_X;
    display.drawLine(x-15,34+by,x-5,38+by,SSD1306_WHITE);
    display.drawLine(x-5,38+by,x+6,38+by,SSD1306_WHITE);
    display.drawLine(x+6,38+by,x+15,34+by,SSD1306_WHITE);
    display.drawLine(x-14,35+by,x-5,39+by,SSD1306_WHITE);
    display.drawLine(x+6,39+by,x+14,35+by,SSD1306_WHITE);
  }
  // Cozy blush freckles; gently glowing like a night-light.
  if((dt/2700)%3!=2){
    display.drawPixel(18,44+by,SSD1306_WHITE);
    display.drawPixel(21,45+by,SSD1306_WHITE);
    display.drawPixel(107,45+by,SSD1306_WHITE);
    display.drawPixel(110,44+by,SSD1306_WHITE);
  }
  // Blanket below the closed eyes without hiding them.
  display.drawRoundRect(7,52+by,114,17,7,SSD1306_WHITE);
  display.drawFastHLine(15,54+by,98,SSD1306_WHITE);
  display.setTextSize(1);
  int z=(int)(3*sinf(dt*.0025f));
  display.setCursor(98,4-z);display.print((dt/3000)%2?F("zZz"):F("Zzz"));
  drawDreamIcon(dt);
  if((dt/850)%2==0){tinySparkle(14,12,3);tinySparkle(116,25,2);}
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
  display.setCursor(49,25); display.print(F("ARUN"));
  display.setCursor(43,38); display.print(F("YOKESH"));
}

// ====================== STATE / TOUCH LOGIC ==========================
void setState(State next, uint32_t now) {
  const State old=state;
  if(old!=next && next!=State::INTRO_BIRTHDAY && next!=State::SECRET_REACTION &&
     (old==State::INTRO_BIRTHDAY || old==State::SECRET_REACTION)) buzzerHalt();
  state=next;
  stateStarted=now;
  if(next!=old) {
    switch(next) {
      case State::INTRO_BOX: buzzerPlay(SoundID::STARTUP,now);break;
      case State::INTRO_BIRTHDAY: buzzerPlay(SoundID::BIRTHDAY,now);nextSecretMelodyAt=now+4800;break;
      case State::WAKING: buzzerPlay(SoundID::WAKE,now);break;
      case State::DROWSY: buzzerPlay(SoundID::SLEEP,now);break;
      case State::MUSIC: buzzerPlay(SoundID::MUSIC,now);break;
      case State::SECRET_REACTION: buzzerPlay(SoundID::SECRET,now);break;
      case State::SLEEP:
        buzzerHalt();
        nextDreamChirp=now+(uint32_t)random(55000,85001);
        break;
      default:break;
    }
  }

  if (next==State::NORMAL) {
    // Only reboot/wake finishing starts a new idle window. An automatic
    // expression or a pet reaction cannot extend the 60-second inactivity.
    if (old==State::INTRO_PROFILE || old==State::INTRO_BIRTHDAY ||
        (old==State::WAKING && !hungryFromSleep))
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
        } else if(dailyGreetingQueued){
          markDailyGreetingShown();setState(State::DAILY_GREETING,now);
        } else {setState(State::NORMAL,now);scheduleExpressions(now);}
      }
      break;
    case State::INTRO_BIRTHDAY:
      if(reached(now,nextSecretMelodyAt) && buzzerEnabled){buzzerPlay(SoundID::BIRTHDAY,now);nextSecretMelodyAt=now+4800;}
      if(dt>=INTRO_BIRTHDAY_MS) {
        if(dailyGreetingQueued){markDailyGreetingShown();setState(State::DAILY_GREETING,now);}
        else {setState(State::NORMAL,now);scheduleExpressions(now);}
      }
      break;
    case State::NORMAL: {
      if(anniversaryQueued) {
        anniversaryQueued=false;
        setState(State::INTRO_BIRTHDAY,now);
        break;
      }
      if(pendingAlien){pendingAlien=false;startSecretAnimation(SecretAnim::ALIEN,now);break;}
      if(pendingRoyal){pendingRoyal=false;startSecretAnimation(SecretAnim::ROYAL,now);break;}
      if(pendingAngel){pendingAngel=false;startSecretAnimation(SecretAnim::ANGEL,now);break;}
      if(pendingFriend){pendingFriend=false;startSecretAnimation(SecretAnim::FRIEND,now);break;}
      if(!nextHungerAt)nextHungerAt=now+HUNGER_INTERVAL_MS;
      if(!hungry && reached(now,nextHungerAt)) {
        hungry=true;hungerAskedAt=now;hungryFromSleep=false;
        buzzerPlay(SoundID::HUNGRY,now);
      }
      if(hungry && now-hungerAskedAt>=HUNGER_PROMPT_MS) {
        hungry=false;nextHungerAt=now+HUNGER_INTERVAL_MS;
        if(hungryFromSleep){hungryFromSleep=false;setState(State::DROWSY,now);break;}
      }
      const uint32_t idle=now-lastInteraction;
      if (AUTO_SLEEP && idle>=IDLE_SLEEP_MS &&
          !hungry) {
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
      if (!hungry && blink==Blink::READY &&
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
      if(reached(now,nextSecretMelodyAt) && buzzerEnabled){buzzerPlay(SoundID::SECRET,now);nextSecretMelodyAt=now+4000;}
      if(dt>=SECRET_CELEBRATE_MS) {setState(State::NORMAL,now);scheduleExpressions(now);}
      break;
    case State::MENU:
      // Menu is controlled by GPIO18/19. Idle sleep is suppressed in menu.
      break;
    case State::CALENDAR:
      if(dt>=CALENDAR_SHOW_MS)setState(State::WEATHER,now);
      break;
    case State::WEATHER:
      if(dt>=WEATHER_SHOW_MS){setState(State::NORMAL,now);scheduleExpressions(now);}
      break;
    case State::FEEDING:
      if(dt>=FEED_MS){setState(State::NORMAL,now);scheduleExpressions(now);}
      break;
    case State::DAILY_GREETING:
      if(dt>=GREETING_SHOW_MS){setState(State::NORMAL,now);scheduleExpressions(now);}
      break;
    case State::PET:
      if(dt>=1150){setState(State::NORMAL,now);scheduleExpressions(now);}
      break;

    case State::DROWSY:
      if (dt>=DROWSY_MS) setState(State::SLEEP,now);
      break;
    case State::SLEEP:
      // Occasionally wake to ask for food, then fall asleep if ignored.
      if(!hungry && nextHungerAt && reached(now,nextHungerAt)) {
        hungry=true;hungerAskedAt=now;hungryFromSleep=true;
        petAfterWake=false;setState(State::WAKING,now);
        buzzerPlay(SoundID::HUNGRY,now);break;
      }
      if(reached(now,nextDreamChirp)) {
        buzzerPlay(SoundID::DREAM,now);
        nextDreamChirp=now+(uint32_t)random(55000,85001);
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
// Touch hardware removed. PET animation remains available for future input types.
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
      if(pet.secrets&(1U<<SEC_FRIEND))drawBeeFriend(now);
      if(hungry){
        display.fillRect(0,0,128,11,SSD1306_BLACK);
        display.setTextSize(1);display.setCursor(1,1);
        display.print(F("HEY "));display.print(savedOwner.substring(0,6));display.print(F("! YUM?"));
        display.fillRect(0,54,128,10,SSD1306_BLACK);
        display.setCursor(16,55);display.print(F("B: SNACK PLS <3"));
      }
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
    case State::SECRET_REACTION:drawSecretEvent(dt);break;
    case State::CALENDAR: drawCalendar(now); break;
    case State::WEATHER:drawWeather(now);break;
    case State::DAILY_GREETING:drawDailyGreeting(dt);break;
    case State::FEEDING:drawFeeding(dt);break;
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
  // GPIO32 remains unused; no touch sensor setup is required.

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
  buzzerInit();  // GPIO27 passive piezo
  updateDisplayContrast(); // use saved level immediately, even during intro
  startIntroAtBoot(millis());
  beginIntroWiFi(millis());
  lastFrame=millis()-FRAME_MS;
}
void loop() {
  uint32_t now=millis();
  updateIntroWiFi(now);     // NTP + Open-Meteo once at startup; then Wi-Fi OFF
  updateDisplayContrast();  // only sends commands when contrast actually changes
  updateButtons(now);        // 2 tactile menu keys, non-blocking and debounced
  buzzerUpdate(now);       // nonblocking PWM tone sequencer
  checkPetDateDuringRuntime(now);
  updateState(now);
  savePetIfNeeded(now);      // NVS wear-aware periodic save
  if (now-lastFrame>=FRAME_MS) {
    lastFrame=now;
    render(now);
  }
}
