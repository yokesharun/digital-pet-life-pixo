# PIXO V1 · Real-device acceptance checklist

## Compile and flash

- [ ] Arduino IDE 2.x / ESP32 3.x core installed; SSD1306, GFX, ArduinoJson 7.x installed.
- [ ] Select ESP32 Dev Module / 4MB-compatible "Huge APP + filesystem" partition; compile **without errors**.
- [ ] USB-C/CP2102 serial works at 115200; preserve NVS (do not erase full flash).
- [ ] OLED starts, normal baby eyes, introductions, random expressions still work.
- [ ] TTP223 GPIO32 triggers hearts/inverted-V petting; GPIO18/19 menus work.
- [ ] Feeding/hunger (5 min, 30 s timeout), sleep (10 min), birthday/secret modes still function.
- [ ] Passive piezo GPIO27 emits the original pet tones.

## First-time setup

- [ ] First boot displays `Pixo-Setup`, random AP password and `192.168.4.1` on OLED.
- [ ] Phone can join WPA2 AP, access setup page over `http://192.168.4.1`.
- [ ] Scanned 2.4GHz SSIDs populate or manual network can be supplied.
- [ ] Invalid/too-short dashboard password rejected; valid data saved; device restarts.
- [ ] Wi-Fi reconnects automatically; browse `http://pixo.local` or actual router-assigned IP.
- [ ] Login with `pixo` / chosen password; wrong password gets 401.
- [ ] OLED Settings → WiFi / IP shows the correct IP, even if `.local` does not resolve.

## Messages

- [ ] Mobile + desktop send 1–200 character messages; longer rejected.
- [ ] Cute notification animation and tone; message pages are readable.
- [ ] Button A dismisses message; return to original pet eyes.
- [ ] Message expires after timeout; history shows delivery status and survives reboot.
- [ ] Delete a single note and clear all history from web page.
- [ ] Test a message while sleeping, during startup, and during a reminder; verify deferral/preemption.

## Reminders

- [ ] Create, edit, enable/disable and delete each repeat type.
- [ ] Once, daily, weekday, weekly recurrence fires at intended local wall-clock time.
- [ ] Close browser entirely and wait for an upcoming reminder: OLED and buzzer still work.
- [ ] Button A dismisses; Button B snoozes exactly 5 minutes.
- [ ] Dismissing does not double-trigger in same minute.
- [ ] Device restart before reminder: schedule and timestamps retained.
- [ ] Reboot after a reminder fired: no duplicate event for the same scheduled occurrence.
- [ ] Disconnect router **after** NTP sync: reminders continue while powered.
- [ ] Restart device with router offline: schedules retained but NOT triggered before valid time.
- [ ] Reconnect router / restore NTP; scheduling resumes and web status reports valid time.

## Dashboard and security

- [ ] Responsive UI on 390px phone, 768px tablet and desktop; no overflow.
- [ ] Light/dark toggle; Home, Messages, Reminders, Controls, Settings all usable.
- [ ] Button mood controls trigger existing expressions, DND defers web alerts, sound switch persists.
- [ ] Timezone and device name persist across reboot; incorrect values rejected.
- [ ] `GET /api/status` requires login; POST/PUT/DELETE missing `X-Pixo-Action` rejected.
- [ ] Wi-Fi reset recreates AP while retaining `pixo` NVS and LittleFS records.
- [ ] Confirm no router port-forwarding or public Internet exposure.
- [ ] Inspect free heap/stack and flash usage on exact hardware during frequent dashboard polling.
- [ ] Run continuously 24–72 hours to assess Wi-Fi reconnection, flash writes and OLED responsiveness.

## Known coverage at delivery

- [x] Original v6.1 source inspected and preserved as integration base.
- [x] No new GPIO assignments; OLED 25/26, buttons 18/19, touch 32, buzzer 27.
- [x] Static HTML/CSS/JS tested in Chromium desktop/mobile using mock API responses.
- [x] JavaScript syntax and gzip round-trip checks.
- [ ] Actual Arduino-ESP32 compile not possible in delivery environment.
- [ ] Physical Wi-Fi setup, button event, mDNS, OLED and buzzer interaction tests must be completed on device.
