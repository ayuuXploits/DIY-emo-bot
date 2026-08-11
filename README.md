```
 ██████████   █████ █████ █████                                         █████               █████   
▒▒███▒▒▒▒███ ▒▒███ ▒▒███ ▒▒███                                         ▒▒███               ▒▒███    
 ▒███   ▒▒███ ▒███  ▒▒███ ███       ██████  █████████████    ██████     ▒███████   ██████  ███████  
 ▒███    ▒███ ▒███   ▒▒█████       ███▒▒███▒▒███▒▒███▒▒███  ███▒▒███    ▒███▒▒███ ███▒▒███▒▒▒███▒   
 ▒███    ▒███ ▒███    ▒▒███       ▒███████  ▒███ ▒███ ▒███ ▒███ ▒███    ▒███ ▒███▒███ ▒███  ▒███    
 ▒███    ███  ▒███     ▒███       ▒███▒▒▒   ▒███ ▒███ ▒███ ▒███ ▒███    ▒███ ▒███▒███ ▒███  ▒███ ███
 ██████████   █████    █████    ██▒▒██████  █████▒███ █████▒▒██████  ██ ████████ ▒▒██████   ▒▒█████ 
▒▒▒▒▒▒▒▒▒▒   ▒▒▒▒▒    ▒▒▒▒▒    ▒▒  ▒▒▒▒▒▒  ▒▒▒▒▒ ▒▒▒ ▒▒▒▒▒  ▒▒▒▒▒▒  ▒▒ ▒▒▒▒▒▒▒▒   ▒▒▒▒▒▒     ▒▒▒▒▒  
                                                                                                    
                                                                                                    
                                                                                                                                                
```                                                                                                                    
                                                                                                                    

<div align="center">

# 🤖 Desk Bot

**An expressive desktop companion on a Wemos D1 Mini**

[![License: Proprietary](https://img.shields.io/badge/License-All_Rights_Reserved-red.svg?style=for-the-badge)](./LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP8266-00979D?style=for-the-badge&logo=arduino&logoColor=white)](https://www.arduino.cc)
[![Display](https://img.shields.io/badge/Display-SSD1306_OLED_128×64-5C5C5C?style=for-the-badge)](https://github.com/adafruit/Adafruit_SSD1306)
[![Language](https://img.shields.io/badge/Language-C%2B%2B_(Arduino)-blue?style=for-the-badge&logo=cplusplus&logoColor=white)](https://www.arduino.cc)
[![NTP](https://img.shields.io/badge/Clock-NTP_Synced-brightgreen?style=for-the-badge)](https://en.wikipedia.org/wiki/Network_Time_Protocol)
[![Weather](https://img.shields.io/badge/Weather-wttr.in_(no_API_key)-orange?style=for-the-badge)](https://wttr.in)


---

## 🎭 Expressions & Modes

| # | Emoji | Mode | What it does | Single Tap | Long Press |
|---|-------|------|--------------|------------|------------|
| 0 | 😊 | **Alive** | Blinks, looks around, yawns | Puppy squint | Pet the bot (big smile) |
| 1 | 💕 | **Love** | Beating heart eyes | — | — |
| 2 | 😠 | **Angry** | Furrowed eyes + fire | Recoil / reject | Full furious shake |
| 3 | 😢 | **Sad** | Crying & tears | — | Comfort (stops tears) |
| 4 | 😵 | **Dizzy** | Spiral eyes + wavy mouth | — | — |
| 5 | 🕐 | **Clock** | Live NTP time display | — | — |
| 6 | 🌤️ | **Weather** | Temp + animated clouds | — | — |
| 7 | 🎮 | **Floppy Bird** | Playable side-scrolling game | — | — |

> **Double-tap** the sensor at any time to cycle to the next mode.  
> After **1 minute** of no interaction, the bot yawns, drifts to sleep, and shows the clock screen automatically. Any touch wakes it back up.

---

## 🛒 Bill of Materials

| Component | Spec | Notes |
|-----------|------|-------|
| **Wemos D1 Mini** | ESP8266-based MCU | Any D1 Mini variant works |
| **OLED Display** | 0.96″ I2C SSD1306, 128×64 | Address `0x3C` (default) |
| **Capacitive Touch Sensor** | TTP223 or compatible | Sensitivity adjustable via onboard pad |
| **Jumper Wires** | Male-to-female | 4× for OLED, 3× for touch |
| **USB Micro Cable** | Data-capable | For flashing — not power-only |

---

## 🔌 Wiring Diagram

```

Wemos D1 Mini
      │
      ├─── 3V3 ──┬──── VCC (OLED)
      │          └──── VCC (Touch)
      │
      ├─── GND ──┬──── GND (OLED)
      │          └──── GND (Touch)
      │
      ├─── D2 (GPIO4) ──── SDA (OLED)
      ├─── D1 (GPIO5) ──── SCL (OLED)
      └─── D5 (GPIO14) ─── SIGNAL (Touch)

```
| Pin | GPIO | Connected To |
|-----|------|--------------|
| `D1` | GPIO5 | OLED SCL |
| `D2` | GPIO4 | OLED SDA |
| `D5` | GPIO14 | Touch SIGNAL |
| `3V3` | — | OLED VCC, Touch VCC |
| `GND` | — | OLED GND, Touch GND |

---

## 💻 Software Setup

### Step 1 — Install Arduino IDE

Download from [arduino.cc/en/software](https://www.arduino.cc/en/software) and install.

---

### Step 2 — Add ESP8266 Board Support

Open **File → Preferences** and paste into *Additional Boards Manager URLs*:

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json


```

Then go to **Tools → Board Manager**, search `esp8266`, and install **ESP8266 by ESP8266 Community**.

---

### Step 3 — Install Libraries

Go to **Sketch → Include Library → Manage Libraries** and install:

| Library | Author | Version |
|---------|--------|---------|
| `Adafruit SSD1306` | Adafruit Industries | latest |
| `Adafruit GFX Library` | Adafruit Industries | latest |
| `ArduinoJson` | Benoit Blanchon | **v6** |

---

### Step 4 — Clone the Repository

``` bash
git clone https://github.com/ayuuXploits/DIY_emo_bot.git
cd DIY_emo_bot
```

To pull future updates:

```bash
git pull origin main
```

Then open **`emo_bot_vMAX.ino`** in Arduino IDE.

---

### Step 5 — Configure the Code

Edit the configuration block at the top of `emo_bot_vMAX.ino`:

```cpp
// ── WiFi ─────────────────────────────────────────────
const char* ssid       = "YOUR_WIFI_SSID";
const char* password   = "YOUR_WIFI_PASSWORD";

// ── Location ─────────────────────────────────────────
const char* PLACE_NAME = "New York City";   // shown on weather screen

#define LATITUDE    40.7128                 // your latitude
#define LONGITUDE  -74.0060                 // your longitude

// ── Timezone ─────────────────────────────────────────
#define TZ_OFFSET_SEC  19800                // UTC+5:30 for IST (see table below)

```

**Common timezone offsets:**

| Timezone | Offset | `TZ_OFFSET_SEC` |
|----------|--------|-----------------|
| IST — India (UTC+5:30) | +5:30 | `19800` |
| UTC | ±0:00 | `0` |
| GMT+1 — Central Europe | +1:00 | `3600` |
| GST — Gulf (UTC+4) | +4:00 | `14400` |
| SGT — Singapore (UTC+8) | +8:00 | `28800` |
| AEST — Australia East (UTC+10) | +10:00 | `36000` |
| EST — US East (UTC−5) | −5:00 | `-18000` |
| PST — US West (UTC−8) | −8:00 | `-28800` |

---

### Step 6 — Board Settings

Go to **Tools** and configure:

```
Board          →  LOLIN(WEMOS) D1 R2 & mini
CPU Frequency  →  80 MHz
Flash Size     →  4M (3M SPIFFS)
Upload Speed   →  921600
Port           →  COM3                      (Windows)
                  /dev/ttyUSB0              (Linux)
                  /dev/cu.usbserial-XXXX    (macOS)
```

---

### Step 7 — Flash

Click **→ Upload** in Arduino IDE and watch the Serial Monitor at `115200` baud for status output.

---

## 🎮 Gesture Reference

| Gesture | Action |
|---------|--------|
| **Double tap** | Cycle to next mode |
| **Single tap** | Mode-specific reaction |
| **Long press** (600 ms+) | Mode-specific interaction |
| **Any touch** | Wake from sleep |

---

## 🔧 Customization

```cpp

// ── Sleep timeout (ms) ────────────────────────────────
const unsigned long SLEEP_TIMEOUT = 60000UL;   // 1 minute

// ── Touch timing (ms) ────────────────────────────────
const unsigned long DOUBLE_TAP_DELAY = 350;    // window for double-tap
const unsigned long LONG_PRESS_TIME  = 600;    // hold duration for long press

// ── Weather refresh (ms) ─────────────────────────────
// 600000UL = 10 minutes
bool needUpdate = (now - lastWeatherUpdate > 600000UL);
```

---

## 🐛 Troubleshooting

<details>
<summary><strong>OLED shows nothing</strong></summary>

Run the I2C scanner below to find your display address. Default is `0x3C` — change to `0x3D` in code if needed. Double-check `SDA → D2` and `SCL → D1`.

```cpp

#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);
}
void loop() {
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C device at: 0x");
      Serial.println(i, HEX);
    }
  }
  delay(5000);
}
```
</details>

<details>
<summary><strong>Clock shows "No WiFi!"</strong></summary>

- ESP8266 only connects to **2.4 GHz** bands — not 5 GHz
- `ssid` and `password` are **case-sensitive**
- Open Serial Monitor at `115200` baud to see live connection output
</details>

<details>
<summary><strong>Clock stuck on "Syncing time..."</strong></summary>

WiFi is connected but NTP hasn't responded yet — wait up to 10 seconds. The code retries automatically every 5 seconds. Ensure your router allows outbound **UDP on port 123**.
</details>

<details>
<summary><strong>Touch sensor not responding</strong></summary>

- Verify the SIGNAL wire is on `D5`
- Open Serial Monitor — mode changes print `Mode: X`
- Try adjusting `LONG_PRESS_TIME` or `DOUBLE_TAP_DELAY` at the top of the file
</details>

<details>
<summary><strong>Upload fails</strong></summary>

1. Install CH340 drivers: [sparks.gogo.co.nz/ch340.html](https://sparks.gogo.co.nz/ch340.html)
2. Try a different USB cable (many cables are power-only)
3. Lower upload speed: **Tools → Upload Speed → 115200**
4. Hold the **FLASH** button on the Wemos while clicking Upload
</details>

---

## 📦 Dependencies

| Library | Repository |
|---------|-----------|
| ESP8266 Arduino Core | [github.com/esp8266/Arduino](https://github.com/esp8266/Arduino) |
| Adafruit SSD1306 | [github.com/adafruit/Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306) |
| Adafruit GFX Library | [github.com/adafruit/Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library) |
| ArduinoJson v6 | [arduinojson.org](https://arduinojson.org) |

> Weather data is provided by [wttr.in](https://wttr.in) — free, open, and **no API key required**.

---

## 📜 License

**© 2026 ayuuXploits — All Rights Reserved.**

This project and its source code are proprietary. You may build it for personal use, but redistribution, modification for commercial purposes, or re-publishing without permission is not allowed.

---

## ⭐ Show Some Love

If you built one, drop a **⭐ star** on the repo — it means a lot!  
Issues and pull requests are welcome.

---
