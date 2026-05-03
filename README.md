# 🤖 Wemos D1 Mini OLED Desk Bot

An expressive desktop companion running on a **Wemos D1 Mini (ESP8266)** with a 128×64 OLED display and capacitive touch sensor. Features animated emotions, a live NTP clock, and real-time weather — all on a tiny screen.



## 😊 Expressions & Modes

| Mode | Expression | Single Tap | Long Press |
|------|-----------|------------|------------|
| 0 | 😊 **Alive** — blinking, looking around, yawning | Puppy squint | Pet the bot (big smile) |
| 1 | 💕 **Love** — beating heart eyes | — | — |
| 2 | 😠 **Angry** — furrowed eyes + fire | Recoil/reject | Full furious shake |
| 3 | 😢 **Sad** — crying tears | — | Comfort (stops tears) |
| 4 | 😵 **Dizzy** — spiral eyes + wavy mouth | — | — |
| 5 | 🕐 **Clock** — live NTP time display | — | — |
| 6 | 🌤️ **Weather** — temp + animated clouds | — | — |

**Double-tap** the sensor at any time to cycle to the next mode.

After **1 minute** of no interaction, the bot yawns, drifts off to sleep, and shows the clock screen automatically. Any touch wakes it back up.

---

## 🛒 Hardware

| Component | Notes |
|-----------|-------|
| Wemos D1 Mini | ESP8266-based |
| 0.96" I2C OLED | SSD1306 chip, 128×64, address `0x3C` |
| Capacitive Touch Sensor | TTP223 or compatible |
| Jumper wires | Male-to-female |
| USB Micro cable | For flashing |

---

## 🔌 Wiring

```
OLED Display (I2C)              Capacitive Touch (TTP223)
──────────────────              ──────────────────────────
VCC    → 3V3                    VCC    → 3V3
GND    → GND                    GND    → GND
SDA    → D2  (GPIO4)            SIGNAL → D5  (GPIO14)
SCL    → D1  (GPIO5)
```

---

## 💻 Software Setup

### 1 — Install Arduino IDE

Download from [arduino.cc/en/software](https://www.arduino.cc/en/software)

### 2 — Add ESP8266 Board Package

Open **File → Preferences** and paste this into *Additional Boards Manager URLs*:

```
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Then go to **Tools → Board Manager**, search `esp8266`, and install **ESP8266 by ESP8266 Community**.

### 3 — Install Libraries

Go to **Sketch → Include Library → Manage Libraries** and install:

```
Adafruit SSD1306       by Adafruit Industries
Adafruit GFX Library   by Adafruit Industries
ArduinoJson            by Benoit Blanchon  (install v6)
```

###Clone the repo to your machine:
```
bashgit clone https://github.com/ayuuXploits/DIY_emo_bot.git
cd DIY_emo_bot.git
To pull the latest changes later (if the repo gets updated):
bashgit pull origin main
Then open emo_bot_vMAX.ino in Arduino IDE.
```

### 4 — Configure the Code

Open `emo_bot_vMAX.ino` and edit the top section:

```cpp
const char* ssid       = "YOUR_WIFI_SSID";      // ← your WiFi name
const char* password   = "YOUR_WIFI_PASSWORD";  // ← your WiFi password
const char* PLACE_NAME = "New York City";        // ← your city name

#define LATITUDE   40.7128    // ← your latitude
#define LONGITUDE -74.0060    // ← your longitude

#define TZ_OFFSET_SEC 19800   // ← timezone offset in seconds (IST default)
```

**Common timezone offsets:**

| Timezone | `TZ_OFFSET_SEC` |
|----------|-----------------|
| IST — India (UTC+5:30) | `19800` |
| UTC | `0` |
| EST — US East (UTC−5) | `-18000` |
| PST — US West (UTC−8) | `-28800` |
| GMT+1 — Central Europe | `3600` |
| GST — Gulf (UTC+4) | `14400` |
| SGT — Singapore (UTC+8) | `28800` |
| AEST — Australia East (UTC+10) | `36000` |

### 5 — Board Settings

Go to **Tools** and set:

```
Board         → LOLIN(WEMOS) D1 R2 & mini
CPU Frequency → 80 MHz
Flash Size    → 4M (3M SPIFFS)
Upload Speed  → 921600
Port          → COM3 (Windows) / /dev/ttyUSB0 (Linux) / /dev/cu.usbserial-XXXX (macOS)
```

### 6 — Upload

Click the **→ Upload** button in Arduino IDE.

---

## 🎮 Usage

| Gesture | Action |
|---------|--------|
| **Double tap** | Cycle to next mode |
| **Single tap** | Mode-specific reaction (see table above) |
| **Long press** (hold 600ms+) | Mode-specific interaction |
| **Any touch** | Wake from sleep |

---

## 🐛 Troubleshooting

### OLED shows nothing
- Run the I2C scanner below to confirm your display address
- Default is `0x3C` — change to `0x3D` in the code if needed
- Double-check SDA → D2 and SCL → D1

```cpp
// I2C Scanner — upload this to find your OLED address
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin(D2, D1);
}
void loop() {
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found: 0x");
      Serial.println(i, HEX);
    }
  }
  delay(5000);
}
```

### Clock shows "No WiFi!"
- ESP8266 only supports **2.4 GHz** — not 5 GHz bands
- `ssid` and `password` are case-sensitive
- Open **Serial Monitor at 115200 baud** to see connection status

### Clock shows "Syncing time..." and stays there
- WiFi is connected but NTP hasn't responded yet — wait up to 10 seconds
- The code retries automatically every 5 seconds
- Make sure your router allows outbound UDP on port 123

### Touch sensor not responding
- Verify SIGNAL wire is on D5
- Open Serial Monitor — mode changes print `Mode: X`
- Try adjusting `LONG_PRESS_TIME` or `DOUBLE_TAP_DELAY` at the top of the file

### Upload fails
- Install CH340 drivers: [sparks.gogo.co.nz/ch340.html](https://sparks.gogo.co.nz/ch340.html)
- Try a different USB cable (some are power-only)
- Lower upload speed: **Tools → Upload Speed → 115200**
- Hold the FLASH button on the Wemos while clicking Upload

---

## 🔧 Customization

### Change sleep timeout
```cpp
const unsigned long SLEEP_TIMEOUT = 60000UL; // 1 minute — change to taste
```

### Change tap timing
```cpp
const unsigned long DOUBLE_TAP_DELAY = 350; // ms window for double-tap
const unsigned long LONG_PRESS_TIME  = 600; // ms hold for long press
```

### Change weather refresh interval
```cpp
bool needUpdate = (now - lastWeatherUpdate > 600000UL); // 600000 = 10 minutes
```

---

## 📦 Dependencies

| Library | Link |
|---------|------|
| ESP8266 Arduino Core | [github.com/esp8266/Arduino](https://github.com/esp8266/Arduino) |
| Adafruit SSD1306 | [github.com/adafruit/Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306) |
| Adafruit GFX Library | [github.com/adafruit/Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library) |
| ArduinoJson v6 | [arduinojson.org](https://arduinojson.org) |

Weather data provided by [Open-Meteo](https://open-meteo.com) (free, no API key needed).

## 📜 License

© 2026 ayuuXploits — All Rights Reserved


---

## ⭐ Support

If you built this, drop a ⭐ on the repo!

Issues and pull requests are welcome.

---

*Made with 💙 — inspired by elik & emo bots*
