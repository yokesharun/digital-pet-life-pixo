#pragma once
// PIXO V1 local web dashboard adapter. Included AFTER the existing pet renderer
// and BEFORE setup() in PixoV1_Dashboard.ino. No GPIO assignments here.
// Arduino-ESP32 3.x + ArduinoJson 7.x.
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include "PixoWebAssets.h"

static WebServer pixoServer(80);
static Preferences pixoWebPrefs;
static bool pixoFSReady=false, pixoMDNS=false, pixoServerStarted=false;
static bool pixoRestartQueued=false;
static uint32_t pixoRestartAt=0,pixoLastReconnectAt=0,pixoLastSyncCheckAt=0,pixoLastSchedulerTick=0;
static String pixoSsid,pixoPass,pixoAdminSalt,pixoAdminHash,pixoAdminName="pixo";
static String pixoDeviceName="PIXO";
static bool pixoDND=false;

static const size_t PIXO_MAX_MESSAGES=20, PIXO_MAX_REMINDERS=16;
static const uint16_t PIXO_MAX_BODY=1200;

struct PixoMessage {uint32_t id=0,created=0; String text; bool delivered=false;};
struct PixoReminder {uint32_t id=0; String title,description,repeat="once"; uint32_t scheduleAt=0,nextAt=0,lastFired=0,snoozeUntil=0; bool enabled=true,pending=false;};
static PixoMessage pixoMessages[PIXO_MAX_MESSAGES];
static PixoReminder pixoReminders[PIXO_MAX_REMINDERS];
static size_t pixoMessageCount=0,pixoReminderCount=0;
static uint32_t pixoNextMessageId=1,pixoNextReminderId=1;
static bool pixoAlertActive=false,pixoAlertIsReminder=false;
static uint32_t pixoAlertId=0,pixoAlertStarted=0,pixoAlertLastQueued=0;
static uint32_t pixoSavedMessagePage=0;

static uint32_t pixoNowEpoch(){time_t t=time(nullptr);return t>1700000000?(uint32_t)t:0;}
static String pixoSHA256(const String &input){
  uint8_t digest[32];
  const mbedtls_md_info_t *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if(!md || mbedtls_md(md,(const unsigned char*)input.c_str(),input.length(),digest)!=0)return "";
  char hex[65];const char *chars="0123456789abcdef";
  for(int i=0;i<32;i++){hex[i*2]=chars[digest[i]>>4];hex[i*2+1]=chars[digest[i]&15];}
  hex[64]=0;return String(hex);
}
static String pixoRandString(size_t n){const char *abc="ABCDEFGHJKLMNPQRSTUVWXYZ23456789";String s;s.reserve(n);for(size_t i=0;i<n;i++)s+=abc[esp_random()%32];return s;}
static bool pixoConstEq(const String&a,const String&b){size_t n=a.length()>b.length()?a.length():b.length();unsigned char v=(unsigned char)(a.length()^b.length());for(size_t i=0;i<n;i++){char x=i<a.length()?a[i]:0,y=i<b.length()?b[i]:0;v|=(unsigned char)(x^y);}return v==0;}
static const char* pixoTZRule(const String&tz){if(tz=="UTC")return "UTC0";if(tz=="Asia/Tokyo")return "JST-9";if(tz=="Europe/London")return "GMT0BST,M3.5.0/1,M10.5.0/2";if(tz=="America/New_York")return "EST5EDT,M3.2.0/2,M11.1.0/2";return "IST-5:30";}
static bool pixoTZSupported(const String&tz){return tz=="Asia/Kolkata"||tz=="UTC"||tz=="Asia/Tokyo"||tz=="Europe/London"||tz=="America/New_York";}
static void pixoApplyTZ(){setenv("TZ",pixoTZRule(pixoTimeZone),1);tzset();}
static String pixoNetIP(){if(WiFi.status()==WL_CONNECTED)return WiFi.localIP().toString();if(pixoSetupAP)return WiFi.softAPIP().toString();return "";}
static void pixoSendJson(JsonDocument &doc,int code=200){String s;serializeJson(doc,s);pixoServer.sendHeader("Cache-Control","no-store");pixoServer.send(code,"application/json; charset=utf-8",s);}
static void pixoError(int code,const String &why){JsonDocument d;d["error"]=why;pixoSendJson(d,code);}
static bool pixoMutationOK(){if(!pixoServer.hasHeader("X-Pixo-Action")||pixoServer.header("X-Pixo-Action")!="1"){pixoError(403,"Missing local action header");return false;}if(pixoServer.hasHeader("Origin")){String origin=pixoServer.header("Origin");String host=pixoServer.hostHeader();if(host.length() && origin!=String("http://")+host){pixoError(403,"Cross-origin changes are blocked");return false;}}return true;}
static bool pixoAuthorized(){
  if(pixoAdminHash.isEmpty()){pixoError(503,"Finish PIXO setup first");return false;}
  String hdr=pixoServer.header("Authorization");
  if(hdr.startsWith("Basic ")){
    String encoded=hdr.substring(6);if(encoded.length()<=180){unsigned char decoded[150];size_t got=0;
      if(mbedtls_base64_decode(decoded,sizeof(decoded)-1,&got,(const unsigned char*)encoded.c_str(),encoded.length())==0){
        decoded[got]=0;String pair=(char*)decoded;int colon=pair.indexOf(':');
        if(colon>0 && pair.substring(0,colon)==pixoAdminName){
          String candidate=pixoSHA256(pixoAdminSalt+":"+pair.substring(colon+1));
          if(pixoConstEq(candidate,pixoAdminHash))return true;
        }
      }
    }
  }
  pixoServer.sendHeader("WWW-Authenticate","Basic realm=\"PIXO local dashboard\", charset=\"UTF-8\"");
  pixoError(401,"Please sign in as pixo with your dashboard password");return false;
}
static bool pixoReadBody(JsonDocument &doc){
  if(!pixoMutationOK())return false;
  String body=pixoServer.arg("plain");
  if(body.length()==0||body.length()>PIXO_MAX_BODY){pixoError(413,"Request must contain JSON (max 1200 bytes)");return false;}
  DeserializationError error=deserializeJson(doc,body);
  if(error||!doc.is<JsonObject>()){pixoError(400,"Invalid JSON object");return false;}
  return true;
}
static void pixoServeCompressed(const uint8_t *data,size_t n,const char *mime){pixoServer.sendHeader("Content-Encoding","gzip");pixoServer.sendHeader("Cache-Control","public, max-age=86400");pixoServer.send_P(200,mime,(const char*)data,n);}
static bool pixoWriteDoc(const char *name,JsonDocument &doc){
  if(!pixoFSReady)return false;
  String temp=String(name)+".tmp",backup=String(name)+".bak";
  File f=LittleFS.open(temp.c_str(),"w");if(!f)return false;
  size_t written=serializeJson(doc,f);f.flush();f.close();
  if(!written){LittleFS.remove(temp.c_str());return false;}
  LittleFS.remove(backup.c_str());
  if(LittleFS.exists(name)&&!LittleFS.rename(name,backup.c_str())){LittleFS.remove(temp.c_str());return false;}
  if(!LittleFS.rename(temp.c_str(),name)){
    if(LittleFS.exists(backup))LittleFS.rename(backup.c_str(),name);
    return false;
  }
  LittleFS.remove(backup.c_str());return true;
}
static bool pixoReadDoc(const char *name,JsonDocument &doc){if(!pixoFSReady)return false;
  File f=LittleFS.open(name,"r");if(!f)f=LittleFS.open((String(name)+".bak").c_str(),"r");
  if(!f)return false;DeserializationError e=deserializeJson(doc,f);f.close();
  if(!e)return true;
  doc.clear();File backup=LittleFS.open((String(name)+".bak").c_str(),"r");
  if(!backup)return false; e=deserializeJson(doc,backup);backup.close();return !e;
}
static bool pixoSaveMessages(){JsonDocument d;JsonArray a=d["items"].to<JsonArray>();d["next_id"]=pixoNextMessageId;for(size_t i=0;i<pixoMessageCount;i++){JsonObject o=a.add<JsonObject>();o["id"]=pixoMessages[i].id;o["text"]=pixoMessages[i].text;o["created_at"]=pixoMessages[i].created;o["delivered"]=pixoMessages[i].delivered;}return pixoWriteDoc("/messages.json",d);}
static bool pixoSaveReminders(){JsonDocument d;JsonArray a=d["items"].to<JsonArray>();d["next_id"]=pixoNextReminderId;for(size_t i=0;i<pixoReminderCount;i++){const auto&r=pixoReminders[i];JsonObject o=a.add<JsonObject>();o["id"]=r.id;o["title"]=r.title;o["description"]=r.description;o["repeat"]=r.repeat;o["schedule_at"]=r.scheduleAt;o["next_at"]=r.nextAt;o["last_fired"]=r.lastFired;o["snooze_until"]=r.snoozeUntil;o["enabled"]=r.enabled;o["pending"]=r.pending;}return pixoWriteDoc("/reminders.json",d);}
static void pixoLoadData(){
  JsonDocument d;
  if(pixoReadDoc("/messages.json",d)){pixoNextMessageId=d["next_id"]|1U;for(JsonObjectConst o:d["items"].as<JsonArrayConst>()){if(pixoMessageCount>=PIXO_MAX_MESSAGES)break;auto &m=pixoMessages[pixoMessageCount++];m.id=o["id"]|0U;m.created=o["created_at"]|0U;m.text=String(o["text"]|"");m.delivered=o["delivered"]|false;if(m.id>=pixoNextMessageId)pixoNextMessageId=m.id+1;}}
  d.clear();
  if(pixoReadDoc("/reminders.json",d)){pixoNextReminderId=d["next_id"]|1U;for(JsonObjectConst o:d["items"].as<JsonArrayConst>()){if(pixoReminderCount>=PIXO_MAX_REMINDERS)break;auto&r=pixoReminders[pixoReminderCount++];r.id=o["id"]|0U;r.title=String(o["title"]|"");r.description=String(o["description"]|"");r.repeat=String(o["repeat"]|"once");r.scheduleAt=o["schedule_at"]|0U;r.nextAt=o["next_at"]|0U;r.lastFired=o["last_fired"]|0U;r.snoozeUntil=o["snooze_until"]|0U;r.enabled=o["enabled"]|true;r.pending=o["pending"]|false;if(r.id>=pixoNextReminderId)pixoNextReminderId=r.id+1;}}
}
static void pixoMessageJSON(JsonObject o,const PixoMessage&m){o["id"]=m.id;o["text"]=m.text;o["created_at"]=m.created;o["delivered"]=m.delivered;}
static void pixoReminderJSON(JsonObject o,const PixoReminder&r){o["id"]=r.id;o["title"]=r.title;o["description"]=r.description;o["repeat"]=r.repeat;o["schedule_at"]=r.scheduleAt;o["next_at"]=r.nextAt;o["last_fired"]=r.lastFired;o["snooze_until"]=r.snoozeUntil;o["enabled"]=r.enabled;o["pending"]=r.pending;}
static PixoReminder* pixoFindReminder(uint32_t id){for(size_t i=0;i<pixoReminderCount;i++)if(pixoReminders[i].id==id)return &pixoReminders[i];return nullptr;}
static PixoMessage* pixoFindMessage(uint32_t id){for(size_t i=0;i<pixoMessageCount;i++)if(pixoMessages[i].id==id)return &pixoMessages[i];return nullptr;}
static uint32_t pixoAdvanceReminder(uint32_t previous,const String &repeat,uint32_t now){
  if(repeat=="once")return 0;
  time_t raw=(time_t)previous;tm local;
  if(!localtime_r(&raw,&local))return 0;
  for(int i=0;i<400;i++){
    local.tm_mday +=repeat=="weekly"?7:1;local.tm_isdst=-1;
    time_t candidate=mktime(&local);if(candidate<=0)return 0;
    if(repeat=="weekdays" && (local.tm_wday==0||local.tm_wday==6))continue;
    if(candidate>(time_t)now)return (uint32_t)candidate;
  }return 0;
}
static void pixoQueueAlert(bool reminder,uint32_t id){
  if(pixoAlertActive && pixoAlertIsReminder && !reminder)return;
  if(pixoAlertActive && pixoAlertIsReminder && reminder)return;
  if(pixoAlertActive && !pixoAlertIsReminder && reminder){
    PixoMessage* interrupted=pixoFindMessage(pixoAlertId);
    if(interrupted){interrupted->delivered=false;pixoSaveMessages();}
  }
  pixoAlertActive=true;pixoAlertIsReminder=reminder;pixoAlertId=id;
  pixoAlertStarted=millis();pixoSavedMessagePage=0;
  if(state==State::SLEEP||state==State::DROWSY){lastInteraction=millis();setState(State::WAKING,millis());}
  if(!pixoDND && buzzerEnabled)buzzerPlay(reminder?SoundID::REMINDER:SoundID::MESSAGE,millis());
}
static void pixoDismissAlert(bool snooze=false){
  if(!pixoAlertActive)return;
  if (activeSound == SoundID::REMINDER ||
    activeSound == SoundID::MESSAGE) {
    buzzerHalt();
  }
  if(pixoAlertIsReminder){PixoReminder *r=pixoFindReminder(pixoAlertId);
    if(r){r->pending=false;r->snoozeUntil=snooze?pixoNowEpoch()+300:0;pixoSaveReminders();}
  }
  pixoAlertActive=false;pixoAlertId=0;
}
static void pixoUpdateNotifications(uint32_t now){
  if(now-pixoLastSchedulerTick<110)return;pixoLastSchedulerTick=now;
  uint32_t epoch=pixoNowEpoch();if(!epoch)return;
  bool touchedStorage=false;
  for(size_t i=0;i<pixoReminderCount;i++){
    PixoReminder&r=pixoReminders[i];if(!r.enabled)continue;
    if(r.pending){if(!pixoDND && !pixoAlertActive && (!r.snoozeUntil||epoch>=r.snoozeUntil))pixoQueueAlert(true,r.id);continue;}
    if(r.snoozeUntil){if(epoch>=r.snoozeUntil){r.pending=true;r.snoozeUntil=0;touchedStorage=true;}else continue;}
    if(!r.nextAt||epoch<r.nextAt)continue;
    const uint32_t fired=r.nextAt;
    // A stale one-time reminder older than 24 h is expired, not loudly fired.
    if(r.repeat=="once" && epoch-fired>86400){r.nextAt=0;touchedStorage=true;continue;}
    if(r.lastFired==fired){r.nextAt=pixoAdvanceReminder(fired,r.repeat,epoch);touchedStorage=true;continue;}
    r.lastFired=fired;r.nextAt=pixoAdvanceReminder(fired,r.repeat,epoch);
    r.pending=true;touchedStorage=true;
  }
  if(touchedStorage)pixoSaveReminders();
  // Preserve boot greetings and OLED menu: notify as soon as PIXO is idle.
  if(isIntroState(state) || state==State::MENU)return;
  if(!pixoDND && !pixoAlertActive){
    for(size_t i=0;i<pixoReminderCount;i++)if(pixoReminders[i].pending && pixoReminders[i].enabled && !pixoReminders[i].snoozeUntil){pixoQueueAlert(true,pixoReminders[i].id);return;}
    // Queue undelivered messages in history order after all reminders.
    for(size_t i=0;i<pixoMessageCount;i++)if(!pixoMessages[i].delivered){
      pixoMessages[i].delivered=true;pixoSaveMessages();pixoQueueAlert(false,pixoMessages[i].id);return;}
  }
  if(pixoAlertActive && !pixoAlertIsReminder){PixoMessage *m=pixoFindMessage(pixoAlertId);
    uint32_t pages=m?(uint32_t)((m->text.length()+75)/76):1;
    if(now-pixoAlertStarted>=max(9000UL,min(27000UL,pages*4300UL+5000UL)))pixoDismissAlert();
  }
}
// Overlay on top of existing OLED framebuffer; original animations continue below.
static void pixoAlertLine(String text,int y){text.replace("\n"," ");if(text.length()>21)text.remove(21);display.setCursor(2,y);display.print(text);}
static void pixoDrawTextWrapped(const String &msg,uint32_t now){
  // SSD1306's default 5x7 GFX font is ASCII-only. Preserve a readable screen
  // even when a dashboard message contains an emoji or a multibyte symbol.
  String clean;clean.reserve(msg.length());
  for(size_t i=0;i<msg.length();i++){
    uint8_t c=(uint8_t)msg[i];
    if(c==10||c==13){clean+=' ';continue;}
    if(c>=32&&c<127){clean+=(char)c;continue;}
    if(c>=0xC0){clean+='*';}
  }
  const uint16_t perPage=76;
  uint16_t pages=(clean.length()+perPage-1)/perPage;if(!pages)pages=1;
  uint16_t current=(uint16_t)(((now-pixoAlertStarted)/3800)%pages);
  size_t start=current*perPage;String page=clean.substring(start,start+perPage);
  for(int l=0;l<4;l++){String line=page.substring(l*19,l*19+19);pixoAlertLine(line,19+l*9);}
  display.setCursor(4,56);display.print(F("A: OK  "));
  display.print(current+1);display.print('/');display.print(pages);
}
bool pixoWebDrawNotification(uint32_t now){
  if(!pixoAlertActive || isIntroState(state) || pixoDND)return false;
  // Let the existing shake-awake eyes play before the incoming card appears.
  if(state==State::WAKING && now-pixoAlertStarted<WAKE_MS)return false;
  display.fillRect(0,0,128,64,SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);display.setTextSize(1);
  display.drawRoundRect(0,0,128,64,4,SSD1306_WHITE);
  display.drawFastHLine(2,14,124,SSD1306_WHITE);
  if(now-pixoAlertStarted<850){
    display.setCursor(12,7);display.print(F("A tiny surprise!"));
    display.setCursor(34,31);display.print(F("<3  *  <3"));return true;
  }
  if(pixoAlertIsReminder){PixoReminder*r=pixoFindReminder(pixoAlertId);
    display.setCursor(9,4);display.print(F("* REMINDER! *"));
    if(r){String data=r->title;if(data.length()>76)data.remove(76);
      for(int i=0;i<4;i++)pixoAlertLine(data.substring(i*19,i*19+19),19+i*9);
    }
    display.setCursor(3,56);display.print(F("A:OK B:+5m"));
  }else{PixoMessage *m=pixoFindMessage(pixoAlertId);
    display.setCursor(11,4);display.print(F("* A NOTE FOR U *"));
    if(m)pixoDrawTextWrapped(m->text,now);
  }
  return true;
}
bool pixoWebButtonShort(uint8_t pin,uint32_t now){
  (void)now;
  if(!pixoAlertActive)return false;
  if(pin==BUTTON_A_PIN){pixoDismissAlert(false);return true;}
  if(pin==BUTTON_B_PIN && pixoAlertIsReminder){pixoDismissAlert(true);return true;}
  if(pin==BUTTON_B_PIN){pixoAlertStarted-=3800;return true;}
  return false;
}

static bool pixoRepeatValid(const String &s){return s=="once"||s=="daily"||s=="weekdays"||s=="weekly";}
static bool pixoFillReminder(PixoReminder &r,JsonDocument&d,bool creating){
  if(d["title"].is<const char*>()){
    String s=d["title"].as<String>();s.trim();if(s.isEmpty()||s.length()>64){pixoError(422,"Title must be 1–64 characters");return false;}r.title=s;
  }else if(creating){pixoError(422,"A title is required");return false;}
  if(d["description"].is<const char*>()){String s=d["description"].as<String>();if(s.length()>140){pixoError(422,"Description must fit 140 characters");return false;}r.description=s;}
  if(d["repeat"].is<const char*>()){String v=d["repeat"].as<String>();if(!pixoRepeatValid(v)){pixoError(422,"Invalid repeat option");return false;}r.repeat=v;}
  if(d["enabled"].is<bool>())r.enabled=d["enabled"].as<bool>();
  if(d["schedule_at"].is<uint32_t>()||d["schedule_at"].is<int64_t>()){
    int64_t t=d["schedule_at"].as<int64_t>();
    if(t<1700000000LL||t>4102444800LL){pixoError(422,"Please choose a valid date");return false;}
    if(creating && pixoNowEpoch() && t<=pixoNowEpoch()){pixoError(422,"Choose a future reminder time");return false;}
    r.scheduleAt=(uint32_t)t;r.nextAt=r.scheduleAt;r.lastFired=0;r.snoozeUntil=0;r.pending=false;
  }else if(creating){pixoError(422,"Choose a date and time");return false;}
  if(r.title.isEmpty()||!pixoRepeatValid(r.repeat)){pixoError(422,"Invalid reminder");return false;}
  return true;
}
static void pixoHandleStatus(){if(!pixoAuthorized())return;
  JsonDocument d;d["connected"]=WiFi.status()==WL_CONNECTED;d["ssid"]=WiFi.status()==WL_CONNECTED?WiFi.SSID():"";d["ip"]=pixoNetIP();d["firmware"]="PIXO V1 Web + Pet 6.1";d["device_name"]=pixoDeviceName;d["hostname"]="pixo.local";
  uint32_t now=pixoNowEpoch();d["now"]=now;d["time_valid"]=now>0;d["timezone"]=pixoTimeZone;
  d["age_days"]=petAgeDays();d["total_pets"]=pet.totalPets;d["affection"]=pet.affection;
  d["state"]=(state==State::SLEEP?"SLEEP":state==State::HEARTS?"HEARTS":state==State::STARRY?"STARRY":state==State::MENU?"MENU":state==State::FEEDING?"FEEDING":"AWAKE");
  d["mood"]=(state==State::SLEEP?"Sleepy":state==State::HEARTS?"Loving":state==State::STARRY?"Excited":state==State::DROWSY?"Dreamy":"Playful");
  d["sound_enabled"]=buzzerEnabled;d["do_not_disturb"]=pixoDND;d["animation_speed"]=pixoAnimationSpeed==0?"calm":pixoAnimationSpeed==2?"playful":"normal";
  uint32_t earliest=UINT32_MAX;PixoReminder *next=nullptr;
  for(size_t i=0;i<pixoReminderCount;i++){auto&r=pixoReminders[i];uint32_t t=r.snoozeUntil?r.snoozeUntil:r.nextAt;if(r.enabled&&t>0&&t<earliest){earliest=t;next=&r;}}
  if(next){JsonObject n=d["next_reminder"].to<JsonObject>();pixoReminderJSON(n,*next);}else d["next_reminder"]=nullptr;
  pixoSendJson(d);
}
static void pixoHandleMessagesGet(){if(!pixoAuthorized())return;JsonDocument d;JsonArray a=d["items"].to<JsonArray>();for(size_t i=0;i<pixoMessageCount;i++){JsonObject o=a.add<JsonObject>();pixoMessageJSON(o,pixoMessages[i]);}pixoSendJson(d);}
static void pixoHandleMessagesPost(){if(!pixoAuthorized())return;JsonDocument d;if(!pixoReadBody(d))return;
  if(!d["text"].is<const char*>()){pixoError(422,"Message text is required");return;}
  String text=d["text"].as<String>();text.trim();
  if(text.isEmpty()||text.length()>200){pixoError(422,"Use 1–200 characters for a tiny note");return;}
  if(!pixoFSReady){pixoError(507,"Flash storage unavailable");return;}
  if(pixoMessageCount>=PIXO_MAX_MESSAGES){
    for(size_t i=1;i<pixoMessageCount;i++)pixoMessages[i-1]=pixoMessages[i];pixoMessageCount--;
  }
  PixoMessage &m=pixoMessages[pixoMessageCount++];m.id=pixoNextMessageId++;m.text=text;m.created=pixoNowEpoch();m.delivered=false;
  if(!pixoSaveMessages()){pixoMessageCount--;pixoError(507,"Could not save the message");return;}
  JsonDocument out;out["ok"]=true;out["id"]=m.id;out["queued"]=true;pixoSendJson(out,201);
}
static void pixoHandleMessagesDeleteAll(){if(!pixoAuthorized()||!pixoMutationOK())return;
  pixoMessageCount=0;if(pixoAlertActive&&!pixoAlertIsReminder)pixoDismissAlert();if(!pixoSaveMessages()){pixoError(507,"Could not clear messages");return;}JsonDocument d;d["ok"]=true;pixoSendJson(d);
}
static void pixoHandleRemindersGet(){if(!pixoAuthorized())return;JsonDocument d;JsonArray a=d["items"].to<JsonArray>();for(size_t i=0;i<pixoReminderCount;i++){JsonObject o=a.add<JsonObject>();pixoReminderJSON(o,pixoReminders[i]);}pixoSendJson(d);}
static void pixoHandleReminderPost(){if(!pixoAuthorized())return;JsonDocument d;if(!pixoReadBody(d))return;if(pixoReminderCount>=PIXO_MAX_REMINDERS){pixoError(507,"16 reminders is the current limit");return;}
  PixoReminder r;if(!pixoFillReminder(r,d,true))return;r.id=pixoNextReminderId++;pixoReminders[pixoReminderCount++]=r;
  if(!pixoSaveReminders()){pixoReminderCount--;pixoError(507,"Could not save the reminder");return;}
  JsonDocument out;out["ok"]=true;out["id"]=r.id;pixoSendJson(out,201);
}
static void pixoHandleSettingsGet(){if(!pixoAuthorized())return;JsonDocument d;d["device_name"]=pixoDeviceName;d["timezone"]=pixoTimeZone;d["ssid"]=WiFi.status()==WL_CONNECTED?WiFi.SSID():"";d["ip"]=pixoNetIP();d["hostname"]="pixo.local";d["firmware"]="PIXO V1 Web + Pet 6.1";d["storage_used"]=pixoFSReady?LittleFS.usedBytes():0;d["storage_total"]=pixoFSReady?LittleFS.totalBytes():0;d["sound_enabled"]=buzzerEnabled;d["do_not_disturb"]=pixoDND;d["animation_speed"]=pixoAnimationSpeed==0?"calm":pixoAnimationSpeed==2?"playful":"normal";d["ota_enabled"]=false;pixoSendJson(d);}
static void pixoHandleSettingsPut(){if(!pixoAuthorized())return;JsonDocument d;if(!pixoReadBody(d))return;
  if(d["device_name"].is<const char*>()){String v=d["device_name"].as<String>();v.trim();if(v.length()<1||v.length()>24){pixoError(422,"Name must have 1–24 characters");return;}pixoDeviceName=v;pixoWebPrefs.putString("name",v);}
  if(d["timezone"].is<const char*>()){String v=d["timezone"].as<String>();if(!pixoTZSupported(v)){pixoError(422,"Unsupported timezone");return;}pixoTimeZone=v;pixoWebPrefs.putString("tz",v);pixoApplyTZ();if(WiFi.status()==WL_CONNECTED)configTzTime(pixoTZRule(v),NTP_SERVER_1,NTP_SERVER_2);}
  JsonDocument out;out["ok"]=true;pixoSendJson(out);
}
static void pixoHandleControls(){if(!pixoAuthorized())return;JsonDocument d;if(!pixoReadBody(d))return;
  if(d["sound_enabled"].is<bool>()){buzzerEnabled=d["sound_enabled"].as<bool>();petPrefs.putBool("sndon",buzzerEnabled);if(!buzzerEnabled)buzzerHalt();}
  if(d["do_not_disturb"].is<bool>()){pixoDND=d["do_not_disturb"].as<bool>();pixoWebPrefs.putBool("dnd",pixoDND);if(pixoDND){
      if(pixoAlertActive&&!pixoAlertIsReminder){PixoMessage*m=pixoFindMessage(pixoAlertId);if(m){m->delivered=false;pixoSaveMessages();}}
      if (activeSound == SoundID::REMINDER) {
  buzzerHalt();
}
      pixoAlertActive=false;/* pending reminders remain stored */
    }}
  if(d["animation_speed"].is<const char*>()){String s=d["animation_speed"].as<String>();if(s!="calm"&&s!="normal"&&s!="playful"){pixoError(422,"Unknown animation speed");return;}pixoAnimationSpeed=s=="calm"?0:s=="playful"?2:1;pixoWebPrefs.putUChar("anim",pixoAnimationSpeed);}
  if(d["mood"].is<const char*>()){
    String mood=d["mood"].as<String>();if(mood!="happy"&&mood!="love"&&mood!="sleepy"&&mood!="excited"){pixoError(422,"Choose happy, love, sleepy or excited");return;}
    uint32_t now=millis();if(!isIntroState(state)){
      lastInteraction=now;
      setState(mood=="happy"?State::DANCE:mood=="love"?State::HEARTS:mood=="sleepy"?State::DROWSY:State::STARRY,now);
    }
  }
  JsonDocument out;out["ok"]=true;pixoSendJson(out);
}
static void pixoHandleReminderPath(){if(!pixoAuthorized())return;
  String url=pixoServer.uri();if(!url.startsWith("/api/reminders/")){pixoError(404,"Not found");return;}
  String part=url.substring(strlen("/api/reminders/"));int slash=part.indexOf('/');String idStr=slash>=0?part.substring(0,slash):part;
  if(idStr.length()<1||idStr.length()>10){pixoError(404,"No such reminder");return;}
  for(size_t i=0;i<idStr.length();i++)if(!isDigit(idStr[i])){pixoError(404,"No such reminder");return;}
  PixoReminder *r=pixoFindReminder((uint32_t)strtoul(idStr.c_str(),nullptr,10));if(!r){pixoError(404,"No such reminder");return;}
  String action=slash>=0?part.substring(slash+1):"";
  if(pixoServer.method()==HTTP_PUT && action.isEmpty()){
    JsonDocument d;if(!pixoReadBody(d))return;PixoReminder backup=*r;if(!pixoFillReminder(*r,d,false))return;
    if(!pixoSaveReminders()){*r=backup;pixoError(507,"Could not save reminder changes");return;}
    JsonDocument out;out["ok"]=true;pixoSendJson(out);return;
  }
  if(pixoServer.method()==HTTP_DELETE && action.isEmpty()){
    if(!pixoMutationOK())return;
    if(pixoAlertActive&&pixoAlertIsReminder&&pixoAlertId==r->id)pixoDismissAlert();
    size_t index=r-pixoReminders;for(size_t i=index+1;i<pixoReminderCount;i++)pixoReminders[i-1]=pixoReminders[i];pixoReminderCount--;
    if(!pixoSaveReminders()){pixoError(507,"Could not delete reminder");return;}
    JsonDocument out;out["ok"]=true;pixoSendJson(out);return;
  }
  if(pixoServer.method()==HTTP_POST && (action=="snooze"||action=="dismiss")){
    if(!pixoMutationOK())return;
    if(action=="snooze"){
      if(!r->pending && !(pixoAlertActive&&pixoAlertIsReminder&&pixoAlertId==r->id)){pixoError(409,"This reminder isn't ringing yet");return;}
      if(!pixoNowEpoch()){pixoError(503,"PIXO needs the correct time first");return;}
      r->pending=false;r->snoozeUntil=pixoNowEpoch()+300;
    }
    else{
      if(!r->pending && !r->snoozeUntil){pixoError(409,"This reminder isn't ringing yet");return;}
      r->pending=false;r->snoozeUntil=0;
    }
if (pixoAlertActive &&
    pixoAlertIsReminder &&
    pixoAlertId == r->id) {

  if (activeSound == SoundID::REMINDER) {
    buzzerHalt();
  }

  pixoAlertActive = false;
  pixoAlertId = 0;
}    if(!pixoSaveReminders()){pixoError(507,"Could not save reminder");return;}
    JsonDocument out;out["ok"]=true;pixoSendJson(out);return;
  }
  pixoError(404,"Unknown reminder action");
}
static void pixoHandleMessagePath(){if(!pixoAuthorized())return;
  String idStr=pixoServer.uri().substring(strlen("/api/messages/"));
  if(pixoServer.method()!=HTTP_DELETE||idStr.isEmpty()||idStr.length()>10){pixoError(404,"Not found");return;}
  for(size_t i=0;i<idStr.length();i++)if(!isDigit(idStr[i])){pixoError(404,"Not found");return;}
  if(!pixoMutationOK())return;
  PixoMessage*m=pixoFindMessage((uint32_t)strtoul(idStr.c_str(),nullptr,10));if(!m){pixoError(404,"No such message");return;}
  if(pixoAlertActive&&!pixoAlertIsReminder&&pixoAlertId==m->id)pixoDismissAlert();
  size_t index=m-pixoMessages;for(size_t i=index+1;i<pixoMessageCount;i++)pixoMessages[i-1]=pixoMessages[i];pixoMessageCount--;
  if(!pixoSaveMessages()){pixoError(507,"Could not delete the message");return;}
  JsonDocument out;out["ok"]=true;pixoSendJson(out);
}

static void pixoStartSetupAP(){
  if(pixoSetupAP)return;
  pixoSetupAP=true;pixoMDNS=false;
  if(pixoApPassword.length()<8){pixoApPassword=pixoRandString(12);pixoWebPrefs.putString("apkey",pixoApPassword);}
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Pixo-Setup",pixoApPassword.c_str());
  Serial.printf("\n[PIXO SETUP] Wi-Fi: Pixo-Setup | password: %s | URL http://192.168.4.1\n",pixoApPassword.c_str());
}
static bool pixoIsSetupClient(){
  IPAddress ip=pixoServer.client().remoteIP();
  return pixoSetupAP&&ip[0]==192&&ip[1]==168&&ip[2]==4;
}
static void pixoSetupNetworks(){
  if(!pixoIsSetupClient()){pixoError(403,"Available only from Pixo-Setup");return;}
  JsonDocument d;JsonArray arr=d["networks"].to<JsonArray>();
  int found=WiFi.scanComplete();
  if(found>=0){
    for(int i=0;i<found&&i<25;i++){
      String ssid=WiFi.SSID(i);if(ssid.isEmpty())continue;
      JsonObject n=arr.add<JsonObject>();n["ssid"]=ssid;n["rssi"]=WiFi.RSSI(i);n["secure"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
  }
  WiFi.scanNetworks(true,true);
  pixoSendJson(d);
}
static void pixoSetupSave(){
  if(!pixoIsSetupClient()||pixoAdminHash.length()>0){pixoError(403,"Setup not available; use Settings to reset Wi-Fi");return;}
  JsonDocument d;if(!pixoReadBody(d))return;
  String ssid=String(d["ssid"]|"");String pw=String(d["password"]|"");String admin=String(d["admin_password"]|"");
  ssid.trim();if(ssid.length()<1||ssid.length()>32||pw.length()>63||admin.length()<10||admin.length()>64){pixoError(422,"Use a valid Wi-Fi name and a 10–64 character dashboard password");return;}
  // Store network credentials in NVS (unprotected on most default ESP32 Dev Boards).
  pixoAdminSalt=pixoRandString(16);pixoAdminHash=pixoSHA256(pixoAdminSalt+":"+admin);
  if(pixoAdminHash.isEmpty()){pixoError(500,"Cannot secure the dashboard");return;}
  bool ok=pixoWebPrefs.putString("ssid",ssid)>0;
  pixoWebPrefs.putString("pass",pw);
  ok=ok&&pixoWebPrefs.putString("salt",pixoAdminSalt)>0;
  ok=ok&&pixoWebPrefs.putString("hash",pixoAdminHash)>0;
  if(!ok){pixoError(507,"Could not save Wi-Fi settings");return;}
  pixoSsid=ssid;pixoPass=pw;
  JsonDocument out;out["ok"]=true;out["restarting"]=true;pixoSendJson(out);
  pixoRestartQueued=true;pixoRestartAt=millis()+2600;
}
static void pixoServeDashboard(){if(pixoSetupAP && pixoIsSetupClient() && pixoAdminHash.isEmpty()){pixoServeCompressed(PIXO_SETUP_HTML,sizeof(PIXO_SETUP_HTML),"text/html; charset=utf-8");return;}
  if(!pixoAuthorized())return;pixoServeCompressed(PIXO_INDEX_HTML,sizeof(PIXO_INDEX_HTML),"text/html; charset=utf-8");}
static void pixoHandleStaticCss(){if(!(pixoSetupAP&&pixoIsSetupClient()&&pixoAdminHash.isEmpty())&&!pixoAuthorized())return;pixoServeCompressed(PIXO_APP_CSS,sizeof(PIXO_APP_CSS),"text/css; charset=utf-8");}
static void pixoHandleSetupJs(){if(!pixoIsSetupClient()||!pixoAdminHash.isEmpty()){pixoError(403,"Not in setup mode");return;}pixoServeCompressed(PIXO_SETUP_JS,sizeof(PIXO_SETUP_JS),"application/javascript; charset=utf-8");}
static void pixoHandleWebJs(){if(!pixoAuthorized())return;pixoServeCompressed(PIXO_APP_JS,sizeof(PIXO_APP_JS),"application/javascript; charset=utf-8");}
static void pixoRestartDevice(){if(!pixoAuthorized()||!pixoMutationOK())return;
  JsonDocument out;out["ok"]=true;pixoSendJson(out);pixoRestartQueued=true;pixoRestartAt=millis()+1000;
}
static void pixoResetWifi(){if(!pixoAuthorized()||!pixoMutationOK())return;
  pixoWebPrefs.remove("ssid");pixoWebPrefs.remove("pass");
  // Changing Wi-Fi requires physical access to the secure setup AP.
  // The existing dashboard password remains valid until reset through setup.
  pixoWebPrefs.remove("hash");pixoWebPrefs.remove("salt");
  JsonDocument out;out["ok"]=true;pixoSendJson(out);pixoRestartQueued=true;pixoRestartAt=millis()+1100;
}
static void pixoHandleNotFound(){
  String path=pixoServer.uri();
  if(path.startsWith("/api/reminders/")){pixoHandleReminderPath();return;}
  if(path.startsWith("/api/messages/")){pixoHandleMessagePath();return;}
  pixoError(404,"PIXO couldn't find that page");
}
void pixoWebBegin(){
  pixoWebPrefs.begin("pixoweb",false);
  pixoSsid=pixoWebPrefs.getString("ssid","");pixoPass=pixoWebPrefs.getString("pass","");
  pixoAdminSalt=pixoWebPrefs.getString("salt","");pixoAdminHash=pixoWebPrefs.getString("hash","");
  pixoApPassword=pixoWebPrefs.getString("apkey","");
  pixoDeviceName=pixoWebPrefs.getString("name","PIXO");
  pixoTimeZone=pixoWebPrefs.getString("tz","Asia/Kolkata");if(!pixoTZSupported(pixoTimeZone))pixoTimeZone="Asia/Kolkata";
  pixoDND=pixoWebPrefs.getBool("dnd",false);
  pixoAnimationSpeed=pixoWebPrefs.getUChar("anim",1);if(pixoAnimationSpeed>2)pixoAnimationSpeed=1;
  pixoApplyTZ();
  pixoFSReady=LittleFS.begin(false);
  if(!pixoFSReady){pixoFSReady=LittleFS.format() && LittleFS.begin(false);}
  if(!pixoFSReady)Serial.println(F("[PIXO] LittleFS failed: messages/reminders cannot persist"));
  else pixoLoadData();
  const char* headers[]={"Authorization","X-Pixo-Action","Origin"};pixoServer.collectHeaders(headers,3);
  pixoServer.on("/",HTTP_GET,pixoServeDashboard);
  pixoServer.on("/app.css",HTTP_GET,pixoHandleStaticCss);
  pixoServer.on("/app.js",HTTP_GET,pixoHandleWebJs);
  pixoServer.on("/setup.js",HTTP_GET,pixoHandleSetupJs);
  pixoServer.on("/setup/networks",HTTP_GET,pixoSetupNetworks);
  pixoServer.on("/setup/save",HTTP_POST,pixoSetupSave);
  pixoServer.on("/api/status",HTTP_GET,pixoHandleStatus);
  pixoServer.on("/api/messages",HTTP_GET,pixoHandleMessagesGet);
  pixoServer.on("/api/messages",HTTP_POST,pixoHandleMessagesPost);
  pixoServer.on("/api/messages",HTTP_DELETE,pixoHandleMessagesDeleteAll);
  pixoServer.on("/api/reminders",HTTP_GET,pixoHandleRemindersGet);
  pixoServer.on("/api/reminders",HTTP_POST,pixoHandleReminderPost);
  pixoServer.on("/api/settings",HTTP_GET,pixoHandleSettingsGet);
  pixoServer.on("/api/settings",HTTP_PUT,pixoHandleSettingsPut);
  pixoServer.on("/api/controls",HTTP_POST,pixoHandleControls);
  pixoServer.on("/api/restart",HTTP_POST,pixoRestartDevice);
  pixoServer.on("/api/wifi/reset",HTTP_POST,pixoResetWifi);
  pixoServer.onNotFound(pixoHandleNotFound);
  if(pixoSsid.isEmpty() || pixoAdminHash.isEmpty()){
    // Lost credentials or first use -> physically accessible WPA2 setup AP.
    if(pixoSsid.isEmpty() && !pixoAdminHash.isEmpty()){pixoWebPrefs.remove("hash");pixoWebPrefs.remove("salt");pixoAdminHash="";}
    pixoStartSetupAP();
  }else{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(pixoSsid.c_str(),pixoPass.c_str());
    pixoLastReconnectAt=millis();
  }
  pixoServer.begin();pixoServerStarted=true;
}
void pixoWebLoop(uint32_t now){
  if(pixoServerStarted)pixoServer.handleClient();
  if(pixoRestartQueued && (int32_t)(now-pixoRestartAt)>=0){savePetIfNeeded(now,true);ESP.restart();}
  if(!pixoSsid.isEmpty()&&WiFi.status()!=WL_CONNECTED){
    if(now-pixoLastReconnectAt>14000){
      pixoLastReconnectAt=now;
      if(!pixoSetupAP && now>30000)pixoStartSetupAP();
      WiFi.begin(pixoSsid.c_str(),pixoPass.c_str());
    }
    if(pixoMDNS){MDNS.end();pixoMDNS=false;}
  }
  if(WiFi.status()==WL_CONNECTED){
    if(!pixoMDNS){pixoMDNS=MDNS.begin("pixo");if(pixoMDNS)MDNS.addService("http","tcp",80);
      Serial.printf("[PIXO] IP: %s; http://pixo.local\n",WiFi.localIP().toString().c_str());}
    if(now-pixoLastSyncCheckAt>1500){
      pixoLastSyncCheckAt=now;
      if(!clockSynced && now>18000 && pixoNowEpoch()==0 && now%30000<1700){
        configTzTime(pixoTZRule(pixoTimeZone),NTP_SERVER_1,NTP_SERVER_2);
      }
      if(!clockSynced && wifiIntroDone() && pixoNowEpoch()){
        clockSynced=true;onClockReady();checkDailyGreeting();checkSecretMilestones();
      }
    }
  }
  pixoUpdateNotifications(now);
  if (pixoAlertActive &&
    pixoAlertIsReminder &&
    !pixoDND &&
    buzzerEnabled &&
    activeSound == SoundID::NONE) {

  buzzerPlay(SoundID::REMINDER, now);
}
}

bool pixoWebButtonLong(uint8_t pin,uint32_t now){(void)pin;(void)now;return pixoAlertActive;}
