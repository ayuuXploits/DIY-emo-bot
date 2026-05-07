#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ================= USER CONFIG =================
const char* ssid      = "YOUR_WIFI_SSID";      // <── your WiFi name
const char* password  = "YOUR_WIFI_PASSWORD";  // <── your WiFi password
const char* PLACE_NAME = "New Delhi";       // City shown on weather screen

// Location for weather (default: New Delhi)
#define LATITUDE   28.7041
#define LONGITUDE  77.1025

// Timezone: IST = UTC+5:30 = 19800 seconds
#define TZ_OFFSET_SEC 19800

// ================= PINS =================
#define TOUCH_PIN D5   // TTP223 signal → D5 (GPIO14), HIGH when touched

// ================= SCREEN =================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// Wire uses D2 (SDA) and D1 (SCL) by default on D1 Mini
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= INPUT STATE MACHINE =================
const int DOUBLE_TAP_DELAY = 350;  // ms between taps for double-tap
const int LONG_PRESS_TIME  = 600;  // ms hold before long press fires

unsigned long touchStartTime = 0;
unsigned long lastTapTime    = 0;
bool isTouching      = false;
bool isLongPressing  = false;
int  tapCount        = 0;

// ================= MODE VARIABLES =================
// 0=Alive  1=Love  2=Angry  3=Sad  4=Dizzy  5=Clock  6=Weather
int  currentMode = 0;
unsigned long lastInteractionTime = 0;
bool isSleeping = false;
const unsigned long SLEEP_TIMEOUT = 60000UL; // 1 minute

// ================= INTERACTION FLAGS =================
// Mode 0
bool isPuppySquint = false;
unsigned long squintEndTime = 0;
bool isBeingPetted = false;

// Mode 2
bool isRejected = false;
unsigned long rejectEndTime = 0;
bool isFurious  = false;

// Mode 3
bool isComforted = false;

// ================= ANIMATION VARS =================
float outdoorTemp  = NAN;
bool  weatherReady = false;
bool  timeReady    = false;
unsigned long lastWeatherUpdate  = 0;
unsigned long lastWeatherAttempt = 0;

// Alive
int  lookDirection = 0;
unsigned long nextLookTime = 0;
bool isYawning   = false;
unsigned long yawnEndTime = 0;
bool hasMidYawned   = false;
bool hasFinalYawned = false;
bool isDriftingOff  = false;
unsigned long randomMidYawnTime = 0;

// Physics
float tearY       = 0;
float spiralAngle = 0.0;
unsigned long lastBlinkTime = 0;
bool  isBlinking    = false;
int   blinkInterval = 2000;
float heartScale    = 1.0;

// Face Geometry
const int BASE_EYE_W = 30;
const int EYE_H      = 44;
const int EYE_Y      = 5;
const int EYE_X_L    = 16;
const int EYE_X_R    = 82;
const int EYE_RADIUS = 8;
const int MOUTH_Y    = 42;

// Tweening state
float currentEyeW_L = BASE_EYE_W, currentEyeW_R = BASE_EYE_W, currentMouthX = 0;
float currentYawnFactor = 0.0, currentEyeOpenFactor = 1.0;
float targetEyeW_L  = BASE_EYE_W, targetEyeW_R = BASE_EYE_W, targetMouthX = 0;
float targetYawnFactor = 0.0, targetEyeOpenFactor = 1.0;

const float PAN_SPEED   = 12.0;
const float YAWN_SPEED  = 0.08;
const float SLEEP_SPEED = 0.05;

// Clouds
unsigned long cloudAnimTimer = 0;
float mainCloudX = -50, smallCloud1X = 20, smallCloud2X = 90;

// ================= PROTOTYPES =================
void handleInput();
void triggerModeChange();
void triggerSingleTapAction();
void triggerLongPressAction();
void releaseLongPress();
void drawEyes();
void drawMouth();
void showClock();
void showWeather();
void fetchWeather();
void syncTime();
void animateClouds();
void drawMainCloud(int x, int y);
void drawSmallCloud(int x, int y);
void drawAngryFire(int x, int y);
void updateAliveAnimations();
void updateHeartbeat();
void triggerYawn(int duration);
void centerText(const char* txt, int y, int size);
void showMessage(const char* msg);

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\nDesk Bot D1 Mini starting..."));

  // TTP223 outputs HIGH when touched — no pull-up needed
  pinMode(TOUCH_PIN, INPUT);

  // I2C on D1 Mini: SDA=D2 (GPIO4), SCL=D1 (GPIO5)
  Wire.begin(D2, D1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 not found — check wiring!"));
    for (;;) yield(); // halt (yield() prevents WDT reset on ESP8266)
  }
  display.setTextColor(SSD1306_WHITE);

  showMessage("Waking up...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting to WiFi"));

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    delay(500);
    Serial.print('.');
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi connected!"));
    syncTime();
    fetchWeather();
  } else {
    Serial.println(F("\nWiFi not connected — clock/weather unavailable."));
  }

  randomSeed(analogRead(A0));
  lastInteractionTime = millis();
  randomMidYawnTime   = random(20000, 40000);
}

// ================= LOOP =================
void loop() {
  unsigned long now = millis();

  // 1. INPUT
  handleInput();

  // 2. SLEEP SEQUENCE (Mode 0 only)
  if (!isSleeping && currentMode == 0 && !isBeingPetted) {
    unsigned long elapsed = now - lastInteractionTime;

    if (!hasMidYawned && elapsed > randomMidYawnTime) {
      triggerYawn(2500); hasMidYawned = true;
    }
    if (!hasFinalYawned && elapsed > (SLEEP_TIMEOUT - 6000)) {
      triggerYawn(3500); hasFinalYawned = true;
    }
    if (!isDriftingOff && elapsed > (SLEEP_TIMEOUT - 2000)) {
      isDriftingOff = true;
      targetEyeOpenFactor = 0.0; targetYawnFactor = 0.0; targetMouthX = 0;
    }
    if (elapsed > SLEEP_TIMEOUT) {
      isSleeping = true;
    }
  }

  // 3. BACKGROUND TASKS
  // WiFi reconnect (non-blocking)
  if (WiFi.status() != WL_CONNECTED && (now - lastWeatherAttempt > 10000)) {
    WiFi.reconnect();
    lastWeatherAttempt = now;
  }

  bool needUpdate  = (now - lastWeatherUpdate > 600000UL); // refresh every 10 min
  bool missingData = (currentMode == 6 && !weatherReady && (now - lastWeatherAttempt > 5000));
  if (WiFi.status() == WL_CONNECTED && (needUpdate || missingData)) {
    fetchWeather();
    lastWeatherAttempt = now;
  }

  // 4. ANIMATION UPDATES
  if (!isSleeping) {
    if (currentMode == 0) updateAliveAnimations();
    if (currentMode == 1) updateHeartbeat();
  }

  // 5. RENDER
  display.clearDisplay();

  if (isSleeping) {
    showClock();
  } else if (currentMode <= 4) {
    drawEyes();
    drawMouth();
  } else if (currentMode == 5) {
    showClock();
  } else if (currentMode == 6) {
    showWeather();
  }

  display.display();

  // yield() keeps the ESP8266 WiFi stack alive — do not remove
  yield();
  delay(20);
}

// ================= INPUT SYSTEM =================
void handleInput() {
  // TTP223: HIGH = touched, LOW = not touched
  bool touch = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  // Rising edge
  if (touch && !isTouching) {
    isTouching = true;
    touchStartTime = now;
    lastInteractionTime = now;

    // Wake from sleep
    if (isSleeping) {
      isSleeping = false;
      isDriftingOff = false;
      targetEyeOpenFactor = 1.0;
      currentMode = 0;
    }
  }

  // Hold
  if (touch && isTouching) {
    if (!isLongPressing && (now - touchStartTime > LONG_PRESS_TIME)) {
      isLongPressing = true;
      triggerLongPressAction();
      tapCount = 0;
    }
  }

  // Falling edge
  if (!touch && isTouching) {
    isTouching = false;
    if (isLongPressing) {
      isLongPressing = false;
      releaseLongPress();
    } else {
      tapCount++;
      lastTapTime = now;
    }
  }

  // Resolve tap count after timeout
  if (!touch && !isLongPressing && tapCount > 0) {
    if (now - lastTapTime > DOUBLE_TAP_DELAY) {
      if      (tapCount == 1) triggerSingleTapAction();
      else if (tapCount >= 2) triggerModeChange();
      tapCount = 0;
    }
  }
}

void triggerModeChange() {
  currentMode++;
  if (currentMode > 6) currentMode = 0;

  // Reset all flags on mode change
  isPuppySquint = false; isBeingPetted = false;
  isRejected    = false; isFurious     = false;
  isComforted   = false;
  isSleeping    = false; isDriftingOff = false;
  hasMidYawned  = false; hasFinalYawned = false;
  currentEyeOpenFactor = targetEyeOpenFactor = 1.0;

  Serial.print(F("Mode: ")); Serial.println(currentMode);
}

void triggerSingleTapAction() {
  if (currentMode == 0) { isPuppySquint = true; squintEndTime = millis() + 1500; }
  else if (currentMode == 2) { isRejected = true; rejectEndTime = millis() + 1000; }
}

void triggerLongPressAction() {
  if (currentMode == 0) isBeingPetted = true;
  if (currentMode == 2) isFurious     = true;
  if (currentMode == 3) isComforted   = true;
}

void releaseLongPress() {
  isBeingPetted = false;
  isFurious     = false;
  isComforted   = false;
}

// ================= ANIMATION LOGIC =================
void triggerYawn(int duration) {
  if (isBeingPetted) return;
  isYawning    = true;
  yawnEndTime  = millis() + duration;
  targetYawnFactor = 1.0;
}

void updateHeartbeat() {
  float beat = sin(millis() * 0.015);
  if      (beat > 0.8) heartScale = 1.2;
  else if (beat > 0.0) heartScale = 1.0 + (beat * 0.2);
  else                 heartScale = 1.0;
}

void updateAliveAnimations() {
  unsigned long now = millis();

  if (isPuppySquint && now > squintEndTime) isPuppySquint = false;
  if (isRejected    && now > rejectEndTime) isRejected    = false;

  // Yawn
  if (isYawning) {
    if (now > yawnEndTime) { isYawning = false; targetYawnFactor = 0.0; }
    else targetYawnFactor = 1.0;
  }

  // Look direction
  bool canLook = !isDriftingOff && currentYawnFactor < 0.1 &&
                 !isYawning && !isBeingPetted && !isPuppySquint;

  if (canLook) {
    if (now > nextLookTime) {
      int r = random(0, 10);
      if      (r < 6) { lookDirection =  0; nextLookTime = now + random(2000, 5000); }
      else if (r < 8) { lookDirection = -1; nextLookTime = now + 600; }
      else            { lookDirection =  1; nextLookTime = now + 600; }
    }
    if      (lookDirection ==  0) { targetEyeW_L = BASE_EYE_W;      targetEyeW_R = BASE_EYE_W;      targetMouthX =   0; }
    else if (lookDirection == -1) { targetEyeW_L = BASE_EYE_W - 14; targetEyeW_R = BASE_EYE_W + 14; targetMouthX = -15; }
    else                          { targetEyeW_L = BASE_EYE_W + 14; targetEyeW_R = BASE_EYE_W - 14; targetMouthX =  15; }
  } else {
    targetEyeW_L = BASE_EYE_W; targetEyeW_R = BASE_EYE_W; targetMouthX = 0;
  }

  // Tween helper (lambda not supported cleanly on all ESP8266 builds — use inline)
  #define MOVE_TOWARDS(cur, tgt, spd) \
    (fabs((cur)-(tgt)) < (spd) ? (tgt) : ((cur) < (tgt) ? (cur)+(spd) : (cur)-(spd)))

  currentEyeW_L       = MOVE_TOWARDS(currentEyeW_L,       targetEyeW_L,       PAN_SPEED);
  currentEyeW_R       = MOVE_TOWARDS(currentEyeW_R,       targetEyeW_R,       PAN_SPEED);
  currentMouthX       = MOVE_TOWARDS(currentMouthX,       targetMouthX,       PAN_SPEED);
  currentYawnFactor   = MOVE_TOWARDS(currentYawnFactor,   targetYawnFactor,   YAWN_SPEED);
  currentEyeOpenFactor= MOVE_TOWARDS(currentEyeOpenFactor,targetEyeOpenFactor,SLEEP_SPEED);

  // Blink
  if (!isDriftingOff && !isBeingPetted && currentYawnFactor < 0.1) {
    if (now - lastBlinkTime > (unsigned long)blinkInterval) {
      isBlinking = true;
      if (now - lastBlinkTime > (unsigned long)(blinkInterval + 150)) {
        isBlinking  = false;
        lastBlinkTime = now;
        blinkInterval = random(800, 3500);
      }
    }
  }
}

// ================= DRAWING =================
void drawHeart(int x, int y, float scale) {
  int r    = 8  * scale;
  int offX = 8  * scale;
  int offY = 5  * scale;
  int triH = 16 * scale;
  display.fillCircle(x - offX, y - offY, r, WHITE);
  display.fillCircle(x + offX, y - offY, r, WHITE);
  display.fillTriangle(x - (r*2), y - offY, x + (r*2), y - offY, x, y + triH, WHITE);
}

void drawSingleTear(int x, int y) {
  display.fillCircle(x, y, 2, WHITE);
  display.fillTriangle(x-1, y, x+1, y, x, y-5, WHITE);
}

void drawTears() {
  if (isComforted) return;
  int startY = 38;
  for (int i = 0; i < 8; i++) {
    int dropOffset = (int)(tearY + i * 8) % 28;
    int currentY   = startY + dropOffset;
    int wobble     = (int)(sin((tearY + i) * 0.4) * 2);
    if (dropOffset < 26) {
      drawSingleTear(31 + wobble, currentY);
      drawSingleTear(97 - wobble, currentY);
    }
  }
  tearY += 0.6;
}

void drawSpiral(int cx, int cy, int dir, float rotOff) {
  float angle = 0, radius = 0;
  while (radius < 14) {
    float ea = (angle + rotOff) * dir;
    int x = cx + (int)(cos(ea) * radius);
    int y = cy + (int)(sin(ea) * radius);
    display.drawPixel(x, y, WHITE);
    display.drawPixel(x+1, y, WHITE);
    angle  += 0.4;
    radius += 0.25;
  }
}

void drawAngryFire(int cx, int bottomY) {
  int frame = (millis() / 100) % 2;
  if (frame == 0) {
    display.fillTriangle(cx-6, bottomY, cx+6, bottomY, cx,   bottomY-14, WHITE);
    display.fillTriangle(cx-9, bottomY-2, cx-5, bottomY-2, cx-7, bottomY-8,  WHITE);
    display.fillTriangle(cx+5, bottomY-2, cx+9, bottomY-2, cx+7, bottomY-8,  WHITE);
  } else {
    display.fillTriangle(cx-7,  bottomY,   cx+7,  bottomY,   cx,   bottomY-16, WHITE);
    display.fillTriangle(cx-11, bottomY-4, cx-7,  bottomY-4, cx-9, bottomY-10, WHITE);
    display.fillTriangle(cx+7,  bottomY-4, cx+11, bottomY-4, cx+9, bottomY-10, WHITE);
  }
}

void drawEyes() {
  // ── MODE 0: ALIVE ──
  if (currentMode == 0) {
    int wL = (int)(currentEyeW_L + 0.5);
    int wR = (int)(currentEyeW_R + 0.5);
    float h = EYE_H;

    if (currentYawnFactor > 0)
      h = map((int)(currentYawnFactor * 100), 0, 100, EYE_H, 6);
    h *= currentEyeOpenFactor;

    if (isPuppySquint)                              h = 10;
    if (isBeingPetted)                              h = 4;
    if (isBlinking && !isDriftingOff && !isBeingPetted) h = 4;
    if (h < 2 && currentEyeOpenFactor > 0.1)       h = 2;

    int finalH = (int)h;
    int lx = (EYE_X_L + BASE_EYE_W/2) - wL/2;
    int rx = (EYE_X_R + BASE_EYE_W/2) - wR/2;
    int ly = EYE_Y + (EYE_H - finalH)/2;
    int ry = ly;

    if (finalH <= 6) {
      display.fillRect(lx, ly, wL, finalH, WHITE);
      display.fillRect(rx, ry, wR, finalH, WHITE);
    } else {
      display.fillRoundRect(lx, ly, wL, finalH, EYE_RADIUS, WHITE);
      display.fillRoundRect(rx, ry, wR, finalH, EYE_RADIUS, WHITE);
    }
    return;
  }

  // ── MODE 2: ANGRY ──
  if (currentMode == 2) {
    int shakeX = 0, shakeY = 0;
    int eyebrowH = 4;
    if (isFurious) {
      shakeX = random(-2, 3); shakeY = random(-2, 3);
      eyebrowH = 12;
      drawAngryFire(64 + shakeX, EYE_Y + 13 + shakeY);
    }
    int angryOffset = isRejected ? -15 : 0;
    display.fillRoundRect(EYE_X_L + shakeX + angryOffset, EYE_Y+18+shakeY, BASE_EYE_W, EYE_H-18, 6, WHITE);
    display.fillRect    (EYE_X_L + shakeX + angryOffset, EYE_Y+18+shakeY, BASE_EYE_W, eyebrowH, BLACK);
    display.fillRoundRect(EYE_X_R + shakeX + angryOffset, EYE_Y+18+shakeY, BASE_EYE_W, EYE_H-18, 6, WHITE);
    display.fillRect    (EYE_X_R + shakeX + angryOffset, EYE_Y+18+shakeY, BASE_EYE_W, eyebrowH, BLACK);
    return;
  }

  // ── OTHER MODES ──
  switch (currentMode) {
    case 1:
      drawHeart(EYE_X_L + BASE_EYE_W/2, EYE_Y + EYE_H/2, heartScale);
      drawHeart(EYE_X_R + BASE_EYE_W/2, EYE_Y + EYE_H/2, heartScale);
      break;
    case 3:
      display.fillRect(EYE_X_L, EYE_Y+28, BASE_EYE_W, 5, WHITE);
      display.fillRect(EYE_X_R, EYE_Y+28, BASE_EYE_W, 5, WHITE);
      drawTears();
      break;
    case 4:
      drawSpiral(EYE_X_L + BASE_EYE_W/2, EYE_Y + EYE_H/2, -1, spiralAngle);
      drawSpiral(EYE_X_R + BASE_EYE_W/2, EYE_Y + EYE_H/2,  1, spiralAngle);
      spiralAngle += 0.3;
      break;
  }
}

void drawMouth() {
  int cx = 64;

  // ── MODE 0: ALIVE ──
  if (currentMode == 0) {
    int x = cx + (int)(currentMouthX + 0.5);
    if (isBeingPetted) {
      display.fillCircle(x, MOUTH_Y+2, 12, WHITE);
      display.fillCircle(x, MOUTH_Y-2, 12, BLACK);
      return;
    }
    if (currentYawnFactor > 0.05) {
      int yW = 16 + (int)(currentYawnFactor * 4);
      int yH = (int)(currentYawnFactor * 18);
      display.fillRoundRect(x - yW/2, MOUTH_Y+5 - yH/2, yW, yH+5, 5, WHITE);
      display.fillRoundRect(x - yW/2+2, MOUTH_Y+5 - yH/2+2, yW-4, yH+5-4, 3, BLACK);
    } else {
      display.fillCircle(x, MOUTH_Y+5, 9, WHITE);
      display.fillCircle(x, MOUTH_Y+1, 9, BLACK);
    }
    return;
  }

  // ── MODE 2: ANGRY ──
  if (currentMode == 2) {
    int shakeX = isFurious ? random(-2, 3) : 0;
    int angryOffset = isRejected ? -15 : 0;
    display.fillRoundRect(cx-12 + shakeX + angryOffset, MOUTH_Y+10, 24, 4, 1, WHITE);
    return;
  }

  switch (currentMode) {
    case 1:
      display.fillCircle(cx, MOUTH_Y+5, 9, WHITE);
      display.fillCircle(cx, MOUTH_Y+1, 9, BLACK);
      break;
    case 3:
      if (isComforted) display.fillRect(cx-8, MOUTH_Y+14, 16, 3, WHITE);
      else {
        display.fillCircle(cx, MOUTH_Y+12, 9, WHITE);
        display.fillCircle(cx, MOUTH_Y+16, 9, BLACK);
      }
      break;
    case 4:
      for (int x = -14; x < 14; x++) {
        int yOff = (int)(sin(x * 0.6) * 3);
        display.fillCircle(cx + x, MOUTH_Y+10 + yOff, 1, WHITE);
      }
      break;
  }
}

// ================= CLOCK =================
void syncTime() {
  // Non-blocking — just kick off NTP, check time(nullptr) later
  configTime(TZ_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(F("NTP sync started"));
}

void showClock() {
  time_t now = time(nullptr);

  // time(nullptr) returns seconds since epoch.
  // Values < 100000 mean NTP hasn't synced yet (epoch ~1970).
  if (now < 100000) {
    if (WiFi.status() != WL_CONNECTED) {
      centerText("No WiFi!", 18, 1);
      centerText("Check SSID &", 30, 1);
      centerText("password in code", 42, 1);
    } else {
      centerText("Syncing time", 20, 1);
      // Animate dots so user knows it is working
      int d = (millis() / 500) % 4;
      char buf[5] = {' ',' ',' ',' '};
      buf[4] = 0;
      for (int i = 0; i < d; i++) buf[i] = '.';
      centerText(buf, 35, 1);
      // Retry NTP every 5 seconds if still not ready
      static unsigned long lastRetry = 0;
      if (millis() - lastRetry > 5000) {
        configTime(TZ_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");
        lastRetry = millis();
      }
    }
    return;
  }

  struct tm t;
  localtime_r(&now, &t);

  // Date row
  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %d %b", &t);
  centerText(dateStr, 0, 1);

  // Large time (12-hour)
  int hr = t.tm_hour % 12;
  if (hr == 0) hr = 12;
  bool pm = t.tm_hour >= 12;
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", hr, t.tm_min);
  display.setTextSize(3);
  int w = strlen(timeStr) * 6 * 3;
  display.setCursor((128 - w) / 2, 18);
  display.print(timeStr);

  // AM/PM
  centerText(pm ? "PM" : "AM", 52, 1);
}

// ================= WEATHER =================
void fetchWeather() {
  WiFiClient client;
  HTTPClient http;

  String url = "http://api.open-meteo.com/v1/forecast?latitude=" +
               String(LATITUDE, 4) + "&longitude=" + String(LONGITUDE, 4) +
               "&current_weather=true";

  // ESP8266 HTTPClient requires a WiFiClient passed in
  if (http.begin(client, url)) {
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, http.getString());
      if (!err) {
        outdoorTemp  = doc["current_weather"]["temperature"].as<float>();
        weatherReady = true;
        lastWeatherUpdate = millis();
        Serial.print(F("Weather: ")); Serial.print(outdoorTemp); Serial.println(F("°C"));
      }
    } else {
      Serial.print(F("Weather HTTP error: ")); Serial.println(code);
    }
    http.end();
  }
}

void showWeather() {
  if (!weatherReady) {
    centerText("Waiting for WiFi...", 28, 1);
    return;
  }
  centerText(PLACE_NAME, 0, 1);
  animateClouds();
  display.setTextSize(2);
  display.setCursor(26, 46);
  display.print(outdoorTemp, 1);
  display.print((char)247); // degree symbol
  display.print("C");
}

void drawMainCloud(int x, int y) {
  display.fillCircle(x+12, y+10, 8,  SSD1306_WHITE);
  display.fillCircle(x+24, y+7,  10, SSD1306_WHITE);
  display.fillCircle(x+36, y+10, 8,  SSD1306_WHITE);
  display.fillCircle(x+18, y+14, 9,  SSD1306_WHITE);
  display.fillCircle(x+30, y+14, 9,  SSD1306_WHITE);
}

void drawSmallCloud(int x, int y) {
  display.fillCircle(x+6,  y+6, 4, SSD1306_WHITE);
  display.fillCircle(x+12, y+4, 5, SSD1306_WHITE);
  display.fillCircle(x+18, y+6, 4, SSD1306_WHITE);
}

void animateClouds() {
  if (millis() - cloudAnimTimer > 30) {
    mainCloudX   += 0.5; smallCloud1X += 0.5; smallCloud2X += 0.5;
    if (mainCloudX   > 140) mainCloudX   = -60;
    if (smallCloud1X > 140) smallCloud1X = -40;
    if (smallCloud2X > 140) smallCloud2X = -50;
    cloudAnimTimer = millis();
  }
  drawMainCloud( (int)mainCloudX,   20);
  drawSmallCloud((int)smallCloud1X, 16);
  drawSmallCloud((int)smallCloud2X, 30);
}

// ================= HELPERS =================
void centerText(const char* txt, int y, int size) {
  display.setTextSize(size);
  int w = strlen(txt) * 6 * size;
  display.setCursor((128 - w) / 2, y);
  display.print(txt);
}

void showMessage(const char* msg) {
  display.clearDisplay();
  centerText(msg, 28, 1);
  display.display();
}
