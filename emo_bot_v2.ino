// ================================================================
//  Desk Bot — ESP8266 / D1 Mini
//  OLED Face + Clock + Weather + Floppy Bird mini-game
//
//  Libraries required (install via Arduino Library Manager):
//    • Adafruit SSD1306  ≥ 2.5
//    • Adafruit GFX      ≥ 1.11
//    • ArduinoJson       ≥ 6.x  (v7 also works)
//    • ESP8266WiFi       (bundled with ESP8266 board package)
//    • ESP8266HTTPClient (bundled with ESP8266 board package)
// ================================================================

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
const char* ssid       = "Airtel_example_9080"; // write ur own wifi name
const char* password   = "example@1234";//write ur own wifi pass
const char* PLACE_NAME = "GAYA JI";

#define LATITUDE      24.7914
#define LONGITUDE     85.0002
#define TZ_OFFSET_SEC 19800   // UTC+5:30 (India)

// ================= PINS =================
#define TOUCH_PIN D5

// ================= SCREEN =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR    0x3C

// NOTE: Wire.begin() is called inside setup(), NOT at global scope.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= INPUT STATE MACHINE =================
const int DOUBLE_TAP_DELAY = 350;
const int LONG_PRESS_TIME  = 600;

unsigned long touchStartTime = 0;
unsigned long lastTapTime    = 0;
bool isTouching     = false;
bool isLongPressing = false;
int  tapCount       = 0;

// ================= MODE VARIABLES =================
// 0=Alive  1=Love  2=Angry  3=Sad  4=Dizzy  5=Clock  6=Weather  7=FloppyBird
int  currentMode = 0;
unsigned long lastInteractionTime = 0;
bool isSleeping = false;
const unsigned long SLEEP_TIMEOUT = 60000UL;

// ================= INTERACTION FLAGS =================
bool isPuppySquint = false;
unsigned long squintEndTime = 0;
bool isBeingPetted = false;
bool isRejected    = false;
unsigned long rejectEndTime = 0;
bool isFurious   = false;
bool isComforted = false;

// ================= ANIMATION VARS =================
float outdoorTemp  = NAN;
bool  weatherReady      = false;
bool  weatherFetchFailed = false;  // FIX: track fetch failure separately
bool  timeReady    = false;
unsigned long lastWeatherUpdate  = 0;
unsigned long lastWeatherAttempt = 0;

int  lookDirection = 0;
unsigned long nextLookTime = 0;
bool isYawning  = false;
unsigned long yawnEndTime = 0;
bool hasMidYawned    = false;
bool hasFinalYawned  = false;
bool isDriftingOff   = false;
unsigned long randomMidYawnTime = 0;

float tearY       = 0;
float spiralAngle = 0.0;
unsigned long lastBlinkTime = 0;
bool  isBlinking    = false;
int   blinkInterval = 2000;
float heartScale    = 1.0;

const int BASE_EYE_W = 30;
const int EYE_H      = 44;
const int EYE_Y      = 5;
const int EYE_X_L    = 16;
const int EYE_X_R    = 82;
const int EYE_RADIUS = 8;
const int MOUTH_Y    = 42;

float currentEyeW_L = BASE_EYE_W, currentEyeW_R = BASE_EYE_W, currentMouthX = 0;
float currentYawnFactor = 0.0, currentEyeOpenFactor = 1.0;
float targetEyeW_L  = BASE_EYE_W, targetEyeW_R = BASE_EYE_W, targetMouthX = 0;
float targetYawnFactor = 0.0, targetEyeOpenFactor = 1.0;

const float PAN_SPEED   = 12.0;
const float YAWN_SPEED  = 0.08;
const float SLEEP_SPEED = 0.05;

unsigned long cloudAnimTimer = 0;
float mainCloudX = -50, smallCloud1X = 20, smallCloud2X = 90;

// ================= FLOPPY BIRD =================
#define BIRD_X        20
#define BIRD_RADIUS    4
#define GRAVITY        0.35f
#define FLAP_VEL      -3.5f
#define PIPE_WIDTH     10
#define PIPE_GAP       22
#define PIPE_SPEED      2
#define PIPE_INTERVAL  55
#define GROUND_Y       60

struct Pipe {
  int  x;
  int  gapTop;
  bool passed;
};

#define MAX_PIPES 3

float birdY;
float birdVel;
int   birdScore;
bool  gameOver;
bool  gameStarted;
Pipe  pipes[MAX_PIPES];
int   pipeCount;
int   frameCount;
int   highScore = 0;

bool wingUp = false;
unsigned long lastWingFlap = 0;

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
void initFloppyBird();
void spawnPipe();
void drawBird(int x, int y);
void drawPipe(Pipe& p);
bool checkCollision(Pipe& p);
void updateFloppyBird();
void drawFloppyBird();

// ================================================================
//  FLOPPY BIRD
// ================================================================
void initFloppyBird() {
  birdY       = SCREEN_HEIGHT / 2.0f;
  birdVel     = 0;
  birdScore   = 0;
  gameOver    = false;
  gameStarted = false;
  pipeCount   = 0;
  frameCount  = 0;
  wingUp      = false;
  for (int i = 0; i < MAX_PIPES; i++) {
    pipes[i] = { SCREEN_WIDTH + 10, 0, false };
  }
}

void spawnPipe() {
  if (pipeCount >= MAX_PIPES) return;
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x > SCREEN_WIDTH + 5) {
      pipes[i].x      = SCREEN_WIDTH;
      pipes[i].gapTop = random(6, GROUND_Y - PIPE_GAP - 6);
      pipes[i].passed = false;
      pipeCount++;
      return;
    }
  }
}

void drawBird(int x, int y) {
  display.fillCircle(x, y, BIRD_RADIUS, WHITE);
  display.fillCircle(x + 2, y - 1, 1, BLACK);
  display.fillTriangle(x + 4, y, x + 4, y + 2, x + 7, y + 1, WHITE);
  if (wingUp) {
    display.fillTriangle(x - 1, y,     x - 4, y - 4, x - 5, y,     WHITE);
  } else {
    display.fillTriangle(x - 1, y + 1, x - 4, y + 4, x - 5, y + 1, WHITE);
  }
}

void drawPipe(Pipe& p) {
  display.fillRect(p.x, 0, PIPE_WIDTH, p.gapTop, WHITE);
  display.fillRect(p.x - 2, p.gapTop - 4, PIPE_WIDTH + 4, 4, WHITE);
  int bottomY = p.gapTop + PIPE_GAP;
  display.fillRect(p.x, bottomY, PIPE_WIDTH, GROUND_Y - bottomY, WHITE);
  display.fillRect(p.x - 2, bottomY, PIPE_WIDTH + 4, 4, WHITE);
}

bool checkCollision(Pipe& p) {
  int bx = BIRD_X;
  int by = (int)birdY;
  int br = BIRD_RADIUS - 1;
  if (by + br >= GROUND_Y || by - br <= 0) return true;
  if (bx + br < p.x || bx - br > p.x + PIPE_WIDTH) return false;
  if (by - br > p.gapTop && by + br < p.gapTop + PIPE_GAP) return false;
  return true;
}

void updateFloppyBird() {
  if (gameOver || !gameStarted) return;
  frameCount++;

  birdVel += GRAVITY;
  birdY   += birdVel;

  if (millis() - lastWingFlap > 200) {
    wingUp = !wingUp;
    lastWingFlap = millis();
  }

  if (frameCount % PIPE_INTERVAL == 0) spawnPipe();

  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x > SCREEN_WIDTH + 5) continue;
    pipes[i].x -= PIPE_SPEED;

    if (!pipes[i].passed && pipes[i].x + PIPE_WIDTH < BIRD_X) {
      pipes[i].passed = true;
      birdScore++;
      if (birdScore > highScore) highScore = birdScore;
    }

    if (pipes[i].x + PIPE_WIDTH < 0) {
      pipes[i].x = SCREEN_WIDTH + 10;
      pipeCount--;
    }

    if (checkCollision(pipes[i])) { gameOver = true; return; }
  }

  if (birdY + BIRD_RADIUS >= GROUND_Y || birdY - BIRD_RADIUS <= 0) {
    gameOver = true;
  }
}

void drawFloppyBird() {
  if (!gameStarted) {
    centerText("FLOPPY BIRD",     5,  1);
    centerText("Tap to flap!",   20,  1);
    centerText("Double-tap=exit",34,  1);

    static float previewY = 32;
    static float previewV = 0;
    previewV += 0.15f;
    previewY += previewV;
    if (previewY > 55) { previewY = 32; previewV = -2; }
    drawBird(BIRD_X + 50, (int)previewY);
    display.drawFastHLine(0, GROUND_Y, SCREEN_WIDTH, WHITE);
    return;
  }

  if (gameOver) {
    centerText("GAME OVER",   5,  1);
    char buf[20];
    sprintf(buf, "Score: %d", birdScore);
    centerText(buf, 20, 1);
    sprintf(buf, "Best:  %d", highScore);
    centerText(buf, 32, 1);
    centerText("Hold=restart", 46, 1);
    display.fillCircle(BIRD_X + 30, 38, BIRD_RADIUS, WHITE);
    display.drawLine(BIRD_X+28, 36, BIRD_X+32, 40, BLACK);
    display.drawLine(BIRD_X+32, 36, BIRD_X+28, 40, BLACK);
    return;
  }

  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x <= SCREEN_WIDTH) drawPipe(pipes[i]);
  }
  display.drawFastHLine(0, GROUND_Y, SCREEN_WIDTH, WHITE);
  drawBird(BIRD_X, (int)birdY);

  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(birdScore);
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\nDesk Bot D1 Mini starting..."));

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(D2, D1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 not found — check wiring!"));
    for (;;) yield();
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
  lastWeatherAttempt  = millis();
  randomMidYawnTime   = random(20000, 40000);
}

// ================================================================
//  LOOP
// ================================================================
void loop() {
  unsigned long now = millis();

  handleInput();

  if (!isSleeping && currentMode == 0 && !isBeingPetted) {
    unsigned long elapsed = now - lastInteractionTime;
    if (!hasMidYawned && elapsed > randomMidYawnTime) {
      triggerYawn(2500);
      hasMidYawned = true;
    }
    if (!hasFinalYawned && elapsed > (SLEEP_TIMEOUT - 6000)) {
      triggerYawn(3500);
      hasFinalYawned = true;
    }
    if (!isDriftingOff && elapsed > (SLEEP_TIMEOUT - 2000)) {
      isDriftingOff        = true;
      targetEyeOpenFactor  = 0.0;
      targetYawnFactor     = 0.0;
      targetMouthX         = 0;
    }
    if (elapsed > SLEEP_TIMEOUT) isSleeping = true;
  }

  // WiFi reconnect watchdog
  if (WiFi.status() != WL_CONNECTED && (now - lastWeatherAttempt > 10000)) {
    WiFi.reconnect();
    lastWeatherAttempt = now;
  }

  // FIX: retry weather if:
  //   (a) periodic 10-minute refresh, OR
  //   (b) in weather mode, data not ready, and either never tried or last attempt
  //       was >5 s ago (covers both "never fetched" and "fetch failed" cases)
  bool needUpdate  = (now - lastWeatherUpdate > 600000UL);
  bool missingData = (currentMode == 6 && !weatherReady &&
                      (lastWeatherAttempt == 0 || now - lastWeatherAttempt > 5000));
  if (WiFi.status() == WL_CONNECTED && (needUpdate || missingData)) {
    fetchWeather();
    lastWeatherAttempt = now;
  }

  if (!isSleeping) {
    if (currentMode == 0) updateAliveAnimations();
    if (currentMode == 1) updateHeartbeat();
    if (currentMode == 7) updateFloppyBird();
  }

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
  } else if (currentMode == 7) {
    drawFloppyBird();
  }

  display.display();
  yield();
  delay(20);
}

// ================================================================
//  INPUT SYSTEM
// ================================================================
void handleInput() {
  bool touch = digitalRead(TOUCH_PIN);
  unsigned long now = millis();

  if (touch && !isTouching) {
    isTouching     = true;
    touchStartTime = now;
    lastInteractionTime = now;

    if (isSleeping) {
      isSleeping          = false;
      isDriftingOff       = false;
      targetEyeOpenFactor = 1.0;
      currentMode         = 0;
    }
  }

  if (touch && isTouching) {
    if (!isLongPressing && (now - touchStartTime > (unsigned long)LONG_PRESS_TIME)) {
      isLongPressing = true;
      triggerLongPressAction();
      tapCount = 0;
    }
  }

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

  if (!touch && !isLongPressing && tapCount > 0) {
    if (now - lastTapTime > (unsigned long)DOUBLE_TAP_DELAY) {
      if      (tapCount == 1) triggerSingleTapAction();
      else if (tapCount >= 2) triggerModeChange();
      tapCount = 0;
    }
  }
}

void triggerModeChange() {
  int prevMode = currentMode;
  currentMode++;
  if (currentMode > 7) currentMode = 0;

  isPuppySquint = false;
  isBeingPetted = false;
  isRejected    = false;
  isFurious     = false;
  isComforted   = false;
  isSleeping    = false;
  isDriftingOff = false;
  hasMidYawned  = false;
  hasFinalYawned = false;
  currentEyeOpenFactor = targetEyeOpenFactor = 1.0;

  if (currentMode == 7) initFloppyBird();

  // FIX: when switching INTO weather mode, allow an immediate fetch attempt
  // by resetting the attempt timer so the 5-second guard doesn't block it
  if (currentMode == 6 && !weatherReady) {
    lastWeatherAttempt = 0;
  }

  Serial.print(F("Mode: "));
  Serial.println(currentMode);
}

void triggerSingleTapAction() {
  if (currentMode == 0) {
    isPuppySquint = true;
    squintEndTime = millis() + 1500;
  } else if (currentMode == 2) {
    isRejected    = true;
    rejectEndTime = millis() + 1000;
  } else if (currentMode == 7) {
    if (!gameStarted) {
      gameStarted = true;
      birdVel     = FLAP_VEL;
    } else if (!gameOver) {
      birdVel    = FLAP_VEL;
      wingUp     = true;
      lastWingFlap = millis();
    }
  }
}

void triggerLongPressAction() {
  if (currentMode == 0) isBeingPetted = true;
  if (currentMode == 2) isFurious     = true;
  if (currentMode == 3) isComforted   = true;
  if (currentMode == 7 && gameOver)   initFloppyBird();
}

void releaseLongPress() {
  isBeingPetted = false;
  isFurious     = false;
  isComforted   = false;
}

// ================================================================
//  ANIMATION LOGIC
// ================================================================
void triggerYawn(int duration) {
  if (isBeingPetted) return;
  isYawning        = true;
  yawnEndTime      = millis() + duration;
  targetYawnFactor = 1.0;
}

void updateHeartbeat() {
  float beat = sin(millis() * 0.015f);
  if      (beat > 0.8f) heartScale = 1.2f;
  else if (beat > 0.0f) heartScale = 1.0f + (beat * 0.2f);
  else                  heartScale = 1.0f;
}

void updateAliveAnimations() {
  unsigned long now = millis();

  if (isPuppySquint && now > squintEndTime) isPuppySquint = false;
  if (isRejected    && now > rejectEndTime) isRejected    = false;

  if (isYawning) {
    if (now > yawnEndTime) { isYawning = false; targetYawnFactor = 0.0; }
    else                    targetYawnFactor = 1.0;
  }

  bool canLook = !isDriftingOff && currentYawnFactor < 0.1f &&
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
    targetEyeW_L = BASE_EYE_W;
    targetEyeW_R = BASE_EYE_W;
    targetMouthX = 0;
  }

  #define MOVE_TOWARDS(cur, tgt, spd) \
    (fabsf((cur) - (tgt)) < (spd) ? (tgt) : ((cur) < (tgt) ? (cur) + (spd) : (cur) - (spd)))

  currentEyeW_L        = MOVE_TOWARDS(currentEyeW_L,        targetEyeW_L,        PAN_SPEED);
  currentEyeW_R        = MOVE_TOWARDS(currentEyeW_R,        targetEyeW_R,        PAN_SPEED);
  currentMouthX        = MOVE_TOWARDS(currentMouthX,        targetMouthX,        PAN_SPEED);
  currentYawnFactor    = MOVE_TOWARDS(currentYawnFactor,    targetYawnFactor,    YAWN_SPEED);
  currentEyeOpenFactor = MOVE_TOWARDS(currentEyeOpenFactor, targetEyeOpenFactor, SLEEP_SPEED);

  if (!isDriftingOff && !isBeingPetted && currentYawnFactor < 0.1f) {
    if (now - lastBlinkTime > (unsigned long)blinkInterval) {
      isBlinking = true;
      if (now - lastBlinkTime > (unsigned long)(blinkInterval + 150)) {
        isBlinking    = false;
        lastBlinkTime = now;
        blinkInterval = random(800, 3500);
      }
    }
  }
}

// ================================================================
//  DRAWING
// ================================================================
void drawHeart(int x, int y, float scale) {
  int r    = (int)(8  * scale);
  int offX = (int)(8  * scale);
  int offY = (int)(5  * scale);
  int triH = (int)(16 * scale);
  display.fillCircle(x - offX, y - offY, r, WHITE);
  display.fillCircle(x + offX, y - offY, r, WHITE);
  display.fillTriangle(x - (r * 2), y - offY,
                       x + (r * 2), y - offY,
                       x,           y + triH, WHITE);
}

void drawSingleTear(int x, int y) {
  display.fillCircle(x, y, 2, WHITE);
  display.fillTriangle(x - 1, y, x + 1, y, x, y - 5, WHITE);
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
  tearY += 0.6f;
}

void drawSpiral(int cx, int cy, int dir, float rotOff) {
  float angle = 0, radius = 0;
  while (radius < 14) {
    float ea = (angle + rotOff) * dir;
    int x = cx + (int)(cos(ea) * radius);
    int y = cy + (int)(sin(ea) * radius);
    display.drawPixel(x,     y, WHITE);
    display.drawPixel(x + 1, y, WHITE);
    angle  += 0.4f;
    radius += 0.25f;
  }
}

void drawAngryFire(int cx, int bottomY) {
  int frame = (millis() / 100) % 2;
  if (frame == 0) {
    display.fillTriangle(cx - 6, bottomY,     cx + 6, bottomY,     cx,     bottomY - 14, WHITE);
    display.fillTriangle(cx - 9, bottomY - 2, cx - 5, bottomY - 2, cx - 7, bottomY - 8,  WHITE);
    display.fillTriangle(cx + 5, bottomY - 2, cx + 9, bottomY - 2, cx + 7, bottomY - 8,  WHITE);
  } else {
    display.fillTriangle(cx - 7,  bottomY,     cx + 7,  bottomY,     cx,     bottomY - 16, WHITE);
    display.fillTriangle(cx - 11, bottomY - 4, cx - 7,  bottomY - 4, cx - 9, bottomY - 10, WHITE);
    display.fillTriangle(cx + 7,  bottomY - 4, cx + 11, bottomY - 4, cx + 9, bottomY - 10, WHITE);
  }
}

void drawEyes() {
  if (currentMode == 0) {
    int wL = (int)(currentEyeW_L + 0.5f);
    int wR = (int)(currentEyeW_R + 0.5f);
    float h = EYE_H;

    if (currentYawnFactor > 0)
      h = (float)map((int)(currentYawnFactor * 100), 0, 100, EYE_H, 6);
    h *= currentEyeOpenFactor;

    if (isPuppySquint)                                      h = 10;
    if (isBeingPetted)                                      h = 4;
    if (isBlinking && !isDriftingOff && !isBeingPetted)     h = 4;
    if (h < 2 && currentEyeOpenFactor > 0.1f)              h = 2;

    int finalH = (int)h;
    int lx = (EYE_X_L + BASE_EYE_W / 2) - wL / 2;
    int rx = (EYE_X_R + BASE_EYE_W / 2) - wR / 2;
    int ly = EYE_Y + (EYE_H - finalH) / 2;
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

  if (currentMode == 2) {
    int shakeX   = 0, shakeY = 0;
    int eyebrowH = 4;
    if (isFurious) {
      shakeX   = random(-2, 3);
      shakeY   = random(-2, 3);
      eyebrowH = 12;
      drawAngryFire(64 + shakeX, EYE_Y + 13 + shakeY);
    }
    int angryOffset = isRejected ? -15 : 0;
    display.fillRoundRect(EYE_X_L + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, EYE_H - 18, 6, WHITE);
    display.fillRect    (EYE_X_L + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, eyebrowH,    BLACK);
    display.fillRoundRect(EYE_X_R + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, EYE_H - 18, 6, WHITE);
    display.fillRect    (EYE_X_R + shakeX + angryOffset, EYE_Y + 18 + shakeY, BASE_EYE_W, eyebrowH,    BLACK);
    return;
  }

  switch (currentMode) {
    case 1:
      drawHeart(EYE_X_L + BASE_EYE_W / 2, EYE_Y + EYE_H / 2, heartScale);
      drawHeart(EYE_X_R + BASE_EYE_W / 2, EYE_Y + EYE_H / 2, heartScale);
      break;
    case 3:
      display.fillRect(EYE_X_L, EYE_Y + 28, BASE_EYE_W, 5, WHITE);
      display.fillRect(EYE_X_R, EYE_Y + 28, BASE_EYE_W, 5, WHITE);
      drawTears();
      break;
    case 4:
      drawSpiral(EYE_X_L + BASE_EYE_W / 2, EYE_Y + EYE_H / 2, -1, spiralAngle);
      drawSpiral(EYE_X_R + BASE_EYE_W / 2, EYE_Y + EYE_H / 2,  1, spiralAngle);
      spiralAngle += 0.3f;
      break;
  }
}

void drawMouth() {
  int cx = 64;

  if (currentMode == 0) {
    int x = cx + (int)(currentMouthX + 0.5f);
    if (isBeingPetted) {
      display.fillCircle(x, MOUTH_Y + 2, 12, WHITE);
      display.fillCircle(x, MOUTH_Y - 2, 12, BLACK);
      return;
    }
    if (currentYawnFactor > 0.05f) {
      int yW = 16 + (int)(currentYawnFactor * 4);
      int yH = (int)(currentYawnFactor * 18);
      display.fillRoundRect(x - yW / 2, MOUTH_Y + 5 - yH / 2, yW, yH + 5, 5, WHITE);
      display.fillRoundRect(x - yW / 2 + 2, MOUTH_Y + 5 - yH / 2 + 2, yW - 4, yH + 5 - 4, 3, BLACK);
    } else {
      display.fillCircle(x, MOUTH_Y + 5, 9, WHITE);
      display.fillCircle(x, MOUTH_Y + 1, 9, BLACK);
    }
    return;
  }

  if (currentMode == 2) {
    int shakeX      = isFurious ? random(-2, 3) : 0;
    int angryOffset = isRejected ? -15 : 0;
    display.fillRoundRect(cx - 12 + shakeX + angryOffset, MOUTH_Y + 10, 24, 4, 1, WHITE);
    return;
  }

  switch (currentMode) {
    case 1:
      display.fillCircle(cx, MOUTH_Y + 5, 9, WHITE);
      display.fillCircle(cx, MOUTH_Y + 1, 9, BLACK);
      break;
    case 3:
      if (isComforted)
        display.fillRect(cx - 8, MOUTH_Y + 14, 16, 3, WHITE);
      else {
        display.fillCircle(cx, MOUTH_Y + 12, 9, WHITE);
        display.fillCircle(cx, MOUTH_Y + 16, 9, BLACK);
      }
      break;
    case 4:
      for (int x = -14; x < 14; x++) {
        int yOff = (int)(sin(x * 0.6f) * 3);
        display.fillCircle(cx + x, MOUTH_Y + 10 + yOff, 1, WHITE);
      }
      break;
  }
}

// ================================================================
//  CLOCK
// ================================================================
void syncTime() {
  configTime(TZ_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println(F("NTP sync started"));
  timeReady = true;
}

void showClock() {
  time_t now = time(nullptr);

  if (now < 100000) {
    if (WiFi.status() != WL_CONNECTED) {
      centerText("No WiFi!",         18, 1);
      centerText("Check SSID &",     30, 1);
      centerText("password in code", 42, 1);
    } else {
      centerText("Syncing time", 20, 1);
      int d = (millis() / 500) % 4;
      char buf[5] = "    ";
      for (int i = 0; i < d; i++) buf[i] = '.';
      centerText(buf, 35, 1);
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

  char dateStr[20];
  strftime(dateStr, sizeof(dateStr), "%a, %d %b", &t);
  centerText(dateStr, 0, 1);

  int  hr = t.tm_hour % 12;
  if (hr == 0) hr = 12;
  bool pm = t.tm_hour >= 12;
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", hr, t.tm_min);
  display.setTextSize(3);
  int w = strlen(timeStr) * 6 * 3;
  display.setCursor((128 - w) / 2, 18);
  display.print(timeStr);

  centerText(pm ? "PM" : "AM", 52, 1);
}

// ================================================================
//  WEATHER
// ================================================================
void fetchWeather() {
  Serial.println(F("Fetching weather..."));
  weatherFetchFailed = false;

  WiFiClient client;
  HTTPClient http;

  // wttr.in plain-text: returns temperature as e.g. "+32°C" or "-5°C"
  // Using lat/lon so it works anywhere without city name issues.
  // &m forces metric (Celsius).
  String url = "http://wttr.in/" +
               String(LATITUDE, 4) + "," + String(LONGITUDE, 4) +
               "?format=%t&m";

  http.setTimeout(8000);  // FIXED: 8-second hard timeout prevents infinite hang

  if (!http.begin(client, url)) {
    weatherFetchFailed = true;
    Serial.println(F("HTTP begin failed"));
    return;
  }

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  Serial.print(F("HTTP code: ")); Serial.println(code);

  if (code == 200) {
    String payload = http.getString();  // FIXED: buffer fully before parsing
    http.end();
    payload.trim();
    Serial.print(F("Payload: ")); Serial.println(payload);

    // Strip leading '+' if present
    if (payload.length() > 0 && payload[0] == '+') payload.remove(0, 1);

    // Find where the number ends — stop at '°' (multi-byte) or 'C' or end
    int cutAt = payload.length();
    for (int i = 0; i < (int)payload.length(); i++) {
      byte c = (byte)payload[i];
      // '°' is 0xC2 0xB0 in UTF-8; 'C' terminates too
      if (c > 127 || c == 'C') { cutAt = i; break; }
    }
    String numStr = payload.substring(0, cutAt);
    numStr.trim();
    Serial.print(F("Temp string: ")); Serial.println(numStr);

    float parsed = numStr.toFloat();
    // Sanity check: valid temp range -50..65°C, and string wasn't empty
    if (numStr.length() > 0 && parsed > -50.0f && parsed < 65.0f) {
      outdoorTemp        = parsed;
      weatherReady       = true;
      weatherFetchFailed = false;
      lastWeatherUpdate  = millis();
      Serial.print(F("Weather OK: ")); Serial.print(outdoorTemp); Serial.println(F("C"));
    } else {
      weatherFetchFailed = true;
      Serial.print(F("Bad payload: ")); Serial.println(payload);
    }
  } else {
    http.end();
    weatherFetchFailed = true;
    Serial.print(F("HTTP error: ")); Serial.println(code);
  }
}

// FIXED: showWeather() now shows the right message for each state
void showWeather() {
  if (!weatherReady) {
    if (WiFi.status() != WL_CONNECTED) {
      centerText("No WiFi!",    10, 1);
      centerText("Check SSID", 24, 1);
      centerText("& password", 36, 1);
      centerText("in code",    48, 1);
    } else if (weatherFetchFailed) {
      centerText("Fetch failed",  16, 1);
      centerText("Retrying...",   30, 1);
      int d = (millis()/500) % 4;
      char buf[5] = "    "; for (int i=0; i<d; i++) buf[i] = '.';
      centerText(buf, 44, 1);
    } else {
      centerText("Getting",  16, 1);
      centerText("weather",  28, 1);
      int d = (millis()/500) % 4;
      char buf[5] = "    "; for (int i=0; i<d; i++) buf[i] = '.';
      centerText(buf, 42, 1);
    }
    return;
  }

  centerText(PLACE_NAME, 0, 1);
  animateClouds();
  display.setTextSize(2);
  display.setCursor(26, 46);
  display.print(outdoorTemp, 1);
  display.print((char)247);
  display.print("C");
}

void drawMainCloud(int x, int y) {
  display.fillCircle(x + 12, y + 10, 8,  SSD1306_WHITE);
  display.fillCircle(x + 24, y +  7, 10, SSD1306_WHITE);
  display.fillCircle(x + 36, y + 10, 8,  SSD1306_WHITE);
  display.fillCircle(x + 18, y + 14, 9,  SSD1306_WHITE);
  display.fillCircle(x + 30, y + 14, 9,  SSD1306_WHITE);
}

void drawSmallCloud(int x, int y) {
  display.fillCircle(x +  6, y + 6, 4, SSD1306_WHITE);
  display.fillCircle(x + 12, y + 4, 5, SSD1306_WHITE);
  display.fillCircle(x + 18, y + 6, 4, SSD1306_WHITE);
}

void animateClouds() {
  if (millis() - cloudAnimTimer > 30) {
    mainCloudX   += 0.5f;
    smallCloud1X += 0.5f;
    smallCloud2X += 0.5f;
    if (mainCloudX   > 140) mainCloudX   = -60;
    if (smallCloud1X > 140) smallCloud1X = -40;
    if (smallCloud2X > 140) smallCloud2X = -50;
    cloudAnimTimer = millis();
  }
  drawMainCloud( (int)mainCloudX,   20);
  drawSmallCloud((int)smallCloud1X, 16);
  drawSmallCloud((int)smallCloud2X, 30);
}

// ================================================================
//  HELPERS
// ================================================================
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

