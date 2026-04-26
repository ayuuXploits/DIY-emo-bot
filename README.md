# 🍡 Anime Cute Face Bot

A viral cute **ekik/emo-style** anime face bot running on **Wemos D1 Mini** with a 128x64 OLED display and capacitive touch sensor. Features huge expressive anime eyes, 7 cute animations, and a **WiFi real-time NTP clock** triggered by long press.

---

## 📸 Expressions

| 😊 Happy | 😐 Blinking | 💕 Love Eyes | 😲 Shocked | 😴 Sleepy | 🤩 Excited | 🥰 Shy |
|---------|-----------|------------|---------|--------|---------|-----|
| Big smile + blush | Auto-winking | Heart pupils | Super wide eyes | ZZZ bobbing | Bouncing + smile | Looking down |

---

## 🛒 Hardware

| Component | Notes |
|-----------|-------|
| Wemos D1 Mini | ESP8266 based |
| 128x64 I2C OLED | SSD1306 chip, address `0x3C` |
| Capacitive Touch Sensor | TTP223 or similar |
| Jumper wires | Male-to-female |
| USB Micro cable | For flashing |

---

## 🔌 Wiring

```
OLED Display (I2C)         Touch Sensor
──────────────────         ─────────────
VCC  → 3V3                 VCC    → 3V3
GND  → GND                 GND    → GND
SDA  → D2  (GPIO4)         SIGNAL → D5  (GPIO14)
SCL  → D1  (GPIO5)
```

---

## 💻 Software Setup

### 1 — Install Arduino IDE

```bash
# macOS (Homebrew)
brew install --cask arduino-ide

# Or download manually
# https://www.arduino.cc/en/software
```

### 2 — Install ESP8266 Board Package

Open Arduino IDE → **File → Preferences**

Paste this URL into **Additional Boards Manager URLs**:

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then go to **Tools → Board Manager**, search `esp8266`, and install **ESP8266 by ESP8266 Community**.

Or via Arduino CLI:

```bash
# Install Arduino CLI (macOS/Linux)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Add ESP8266 board package
arduino-cli config init
arduino-cli config add board_manager.additional_urls http://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266
```

### 3 — Install Required Libraries

Go to **Sketch → Include Library → Manage Libraries**

Search and install:

```
Adafruit SSD1306      by Adafruit Industries
Adafruit GFX Library  by Adafruit Industries
```

Or via Arduino CLI:

```bash
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
```

### 4 — Clone This Repo

```bash
git clone https://github.com/ayuuXploits/anime-cute-face-bot.git
cd anime-cute-face-bot
```

### 5 — Configure WiFi + Timezone

Open `mochi_bot_wifi_clock.ino` and edit lines **29–35**:

```cpp
// ── WiFi credentials ──────────────────────────────────
const char* ssid     = "YOUR_SSID";       // ← your WiFi name
const char* password = "YOUR_PASSWORD";   // ← your WiFi password

// ── Timezone offset in seconds ────────────────────────
const long gmtOffset_sec      = 19800;   // IST = UTC+5:30 → 5.5×3600
const int  daylightOffset_sec = 0;
```

**Common timezone offsets:**

| Timezone | gmtOffset_sec |
|----------|--------------|
| UTC | `0` |
| IST — India (UTC+5:30) | `19800` |
| EST — US East (UTC−5) | `-18000` |
| CST — US Central (UTC−6) | `-21600` |
| PST — US West (UTC−8) | `-28800` |
| GMT+1 — Central Europe | `3600` |
| GST — Gulf (UTC+4) | `14400` |
| SGT — Singapore (UTC+8) | `28800` |
| AEST — Australia East (UTC+10) | `36000` |
| NZST — New Zealand (UTC+12) | `43200` |

### 6 — Select Board Settings in Arduino IDE

Go to **Tools** and set:

```
Board         → LOLIN(WEMOS) D1 mini (ESP8266)
CPU Frequency → 80 MHz
Flash Size    → 4M (3M SPIFFS)
Upload Speed  → 921600
Port          → /dev/cu.usbserial-XXXX   (macOS)
                /dev/ttyUSB0             (Linux)
                COM3                     (Windows)
```

### 7 — Upload

Click the **→ Upload** button, or via Arduino CLI:

```bash
# Compile
arduino-cli compile --fqbn esp8266:esp8266:d1_mini mochi_bot_wifi_clock.ino

# Upload (replace port with yours)
arduino-cli upload --fqbn esp8266:esp8266:d1_mini --port /dev/cu.usbserial-XXXX mochi_bot_wifi_clock.ino
```

### 8 — Monitor Serial Output

Open **Tools → Serial Monitor** and set baud rate to `115200`

Or via terminal:

```bash
# macOS / Linux using screen
screen /dev/cu.usbserial-XXXX 115200

# Linux using minicom
minicom -b 115200 -D /dev/ttyUSB0

# Exit screen: press Ctrl+A then K
```

Expected boot output:

```
ANIME EYES BOT Starting...
Connecting to WiFi...
..........
WiFi connected!
IP address: 192.168.x.x
Waiting for NTP time sync...
****
Time synchronized!
Setup complete! Touch sensor ready on pin D5
```

---

## 🎮 Usage

| Action | Result |
|--------|--------|
| **Short Tap** (< 800 ms) | Cycle to next anime expression |
| **Long Press** (> 800 ms) | Toggle real-time WiFi clock |
| **Long Press again** | Return to anime expressions |

### Expression Cycle Order

```
Tap 1 → 😊 HAPPY
Tap 2 → 😐 BLINKING
Tap 3 → 💕 LOVE EYES
Tap 4 → 😲 SHOCKED
Tap 5 → 😴 SLEEPY
Tap 6 → 🤩 EXCITED
Tap 7 → 🥰 SHY
Tap 8 → 😊 HAPPY  (loops back)
```

### Clock Display (Long Press)

```
┌────────────────────────────┐
│     12:34               🕐 │
│                            │
│  Sun 27 Apr 2026           │
│  WiFi: Connected           │
└────────────────────────────┘
```

---

## 📁 File Structure

```
anime-cute-face-bot/
├── emo_bot.ino   ← Main sketch (upload this)
├── README.md
└── LICENSE
```

---

## 🐛 Troubleshooting

### OLED shows nothing

```
1. Run I2C Scanner below to find your display address
2. Default is 0x3C — change to 0x3D in setup() if needed
3. Double-check SDA→D2 and SCL→D1
```

**I2C Scanner — paste and upload to find OLED address:**

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
      Serial.print("Found address: 0x");
      Serial.println(i, HEX);
    }
  }
  delay(5000);
}
```

### Touch sensor not responding

```
1. Verify SIGNAL → D5
2. Check VCC and GND on sensor
3. Open Serial Monitor — each tap prints "Face changed to: X"
4. Adjust threshold: change 800 to 600 in the pressDuration check
```

### WiFi won't connect

```
1. ESP8266 only supports 2.4 GHz — not 5 GHz
2. ssid and password are case-sensitive
3. Check Serial Monitor for dot progress while connecting
4. Move Wemos closer to router during first test
```

### Clock shows wrong time

```
1. Confirm gmtOffset_sec matches your timezone (see table above)
2. WiFi must be connected for NTP sync
3. Wait up to 10 seconds after WiFi connects for time to sync
```

### Upload fails

```
1. Install CH340 drivers:
   https://sparks.gogo.co.nz/ch340.html

2. Try a different USB cable (some are power-only)

3. Hold FLASH button on Wemos while clicking Upload

4. Lower upload speed: Tools → Upload Speed → 115200
```

---

## 🔧 Customization

### Change long-press threshold

```cpp
// In loop() — default is 800ms
if (pressDuration > 800) {
```

### Change animation speed

```cpp
// Smaller number = faster animation
float bounce = sin((elapsed / 300.0) * PI) * 3;
//                          ↑ change this
```

### Add a new expression

```cpp
// 1. Add to enum
enum AnimationState { HAPPY, BLINKING, ..., MY_FACE };

// 2. Write the function
void drawMyFace() {
  drawAnimeEye(28, 20, 0);
  drawAnimeEye(100, 20, 0);
  // your mouth / effects here
}

// 3. Add to switch in loop()
case MY_FACE: drawMyFace(); break;

// 4. Increase modulo from 7 to 8
currentAnimation = (AnimationState)((currentAnimation + 1) % 8);
```

---

## 📦 Dependencies

| Library | Link |
|---------|------|
| ESP8266 Arduino Core | [github.com/esp8266/Arduino](https://github.com/esp8266/Arduino) |
| Adafruit SSD1306 | [github.com/adafruit/Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306) |
| Adafruit GFX Library | [github.com/adafruit/Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library) |

---

## 📜 License

© 2026 ayuuXploits — All Rights Reserved


---

## ⭐ Support

If you built this, drop a ⭐ on the repo!

Issues and pull requests are welcome.

---

*Made with 💙 — inspired by ekik & emo bots*
