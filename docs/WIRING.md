# PIXO V1 · Wiring and pin reference

| Source | 30-pin ESP32 Dev Board GPIO | Notes |
| --- | --- | --- |
| OLED SDA | GPIO25 | Preserve previous SSD1306 I2C custom wiring |
| OLED SCL | GPIO26 | Preserve previous SSD1306 I2C custom wiring |
| OLED VCC | 3V3 | SSD1306 3.3V-friendly breakout |
| OLED GND | GND | Ground common to all |
| TTP223 VCC | 3V3 | TTP223 capacitive touch module, momentary output |
| TTP223 OUT | GPIO32 | Active HIGH, internal code debounce 18ms |
| TTP223 GND | GND | Required |
| Button A | GPIO18 → one electrical side of switch, other side → GND | `INPUT_PULLUP`, no external resistor |
| Button B | GPIO19 → one electrical side of switch, other side → GND | `INPUT_PULLUP`, no external resistor |
| Low-current passive piezo + | GPIO27 | Verify current; driver transistor required for higher-current modules |
| Piezo − | GND | Common ground |
| Power | USB-C | No battery sensing or charging circuit |

## 4-leg DIP tactile buttons

The two legs on one **side** of many square tactile buttons are internally common; the other side is common, and pressing joins the sides. Use a multimeter in continuity mode to confirm which two legs close only when pressed. On a breadboard put the switch across the central trench. **Do not** wire GPIO18/19 to 3.3V with `INPUT_PULLUP` mode.

There is **no new GPIO pin** used for Wi-Fi or the web dashboard. The 30-pin and 38-pin boards both expose these GPIO numbers on common ESP32 DevKit variants, but their physical header placements differ. Always follow your board's printed GPIO labels.
