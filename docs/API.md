# PIXO V1 · Local REST API

Origin: `http://pixo.local` or `http://<actual-ESP32-IP>` while on the same LAN.

**Authentication:** HTTP Basic for all dashboard API endpoints, using `pixo` + the setup dashboard password. Changes require `X-Pixo-Action: 1`; POST/PUT JSON also require `Content-Type: application/json`. No CORS headers, no public Internet support, no external services.

## GET /api/status

Returns `{connected,ssid,ip,hostname,firmware,device_name,now,time_valid,timezone,age_days,total_pets,affection,state,mood,sound_enabled,do_not_disturb,animation_speed,next_reminder}`. `next_reminder` may be `null`.

## Messages

| Method and path | JSON body | Result |
| --- | --- | --- |
| GET `/api/messages` | none | `{items:[{id,text,created_at,delivered}]}` |
| POST `/api/messages` | `{"text":"Good morning, PIXO ♡"}` | 201 `{ok:true,id,queued:true}` |
| DELETE `/api/messages/123` | none | `{ok:true}` |
| DELETE `/api/messages` | none | `{ok:true}` clears history |

`text` is 1–200 characters. Queued messages are shown after the intro and when DND is off; arrival or display may be deferred by a pending reminder. `delivered` indicates queued display began, not proof the human read it.

## Reminders

| Method and path | JSON body | Result |
| --- | --- | --- |
| GET `/api/reminders` | none | `{items:[...]}` |
| POST `/api/reminders` | `{"title":"Drink water","description":"Tiny sip","schedule_at":1793808000,"repeat":"daily","enabled":true}` | 201 `{ok:true,id}` |
| PUT `/api/reminders/123` | any of title, description, schedule_at, repeat, enabled | `{ok:true}` |
| DELETE `/api/reminders/123` | none | `{ok:true}` |
| POST `/api/reminders/123/snooze` | none | `{ok:true}`, 5-minute snooze while ringing |
| POST `/api/reminders/123/dismiss` | none | `{ok:true}`, acknowledge ringing reminder |

Repeat values: `once`, `daily`, `weekdays`, `weekly`. `schedule_at`, `next_at`, `last_fired`, `snooze_until` are UTC Unix timestamps (seconds). When firing, `last_fired` and `pending` are persisted before display to prevent double firing on reboot. Daily and weekday schedules follow device-local time. Unavailable or stale NTP means reminders remain in flash but do not fire without valid clock.

## Controls

`POST /api/controls` with any of `{"mood":"happy|love|sleepy|excited","sound_enabled":true,"do_not_disturb":false,"animation_speed":"calm|normal|playful"}`. These map to the existing pet states, buzzer Preferences, DND, and random expression frequency.

## Settings

- `GET /api/settings` gives device_name, timezone, SSID, IP, firmware, local FS usage, controls and `ota_enabled:false`.
- `PUT /api/settings` accepts `{"device_name":"PIXO","timezone":"Asia/Kolkata"}`. Supported timezones: Asia/Kolkata, UTC, Asia/Tokyo, Europe/London, America/New_York.
- `POST /api/restart` restarts the ESP32 after a response.
- `POST /api/wifi/reset` removes only dashboard Wi-Fi and admin settings, then starts the WPA2 setup AP after restart (pet memories/messages/reminders unchanged).

## Setup mode (only on the `Pixo-Setup` AP)

- `GET /setup/networks`: scan nearby SSIDs; may return an empty list during an asynchronous scan, so retry after 1–2 seconds.
- `POST /setup/save`: `{"ssid":"your-2.4GHz-network","password":"Wi-Fi-password","admin_password":"a-separate-long-dashboard-password"}`. Creates salted password verifier and stores network config; restarts PIXO.

## Errors

JSON errors follow `{ "error": "human-readable message" }`. Typical status codes: `400` malformed JSON, `401` auth required, `403` forbidden mutation or origin, `404` missing ID, `409` reminder is not ringing, `413` body over 1200 bytes, `422` validation failure, `503` clock/setup unavailable, `507` persistence failed.

## cURL example

```bash
curl -u pixo:'YOUR_DASHBOARD_PASSWORD' -H 'Content-Type: application/json' -H 'X-Pixo-Action: 1' -d '{"text":"Hi PIXO!"}' http://pixo.local/api/messages
```

Avoid embedding the password in shell history on shared computers. Never port-forward the API.
