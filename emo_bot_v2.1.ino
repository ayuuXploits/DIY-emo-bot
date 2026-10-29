// ================================================================
//  Desk Bot — ESP8266 / D1 Mini
//  OLED Face + Clock + Weather + Floppy Bird + Pantheon Gateway UI
// ================================================================

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ================= CONSTANTS & DEFAULTS =================
#define TZ_OFFSET_SEC 19800   // UTC+5:30 (India)

// Access Point credentials (for setup mode)
const char* AP_SSID = "EmoBot-Gateway";
const char* AP_PASS = "12345678";

// ================= EEPROM CONFIG STORAGE =================
#define EEPROM_MAGIC 0x65  // Configuration signature

struct BotConfig {
  byte  magic;
  char  ssid[33];
  char  password[65];
  char  placeName[33];
  float latitude;
  float longitude;
};

BotConfig botConfig;
bool isApModeActive = false;

ESP8266WebServer server(80);

// ================= PINS =================
#define TOUCH_PIN D5

// ================= SCREEN =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR    0x3C

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
float outdoorTemp        = NAN;
bool  weatherReady       = false;
bool  weatherFetchFailed = false;
bool  timeReady          = false;
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
void setup();
void loop();
void loadConfig();
void saveConfig();
void resetConfig();
void setupWebServer();
void startAccessPoint();
void connectToWiFi();
void showSetupScreen();
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
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\nPantheon Node Booting..."));

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(D2, D1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED hardware allocation failed!"));
    for (;;) yield();
  }
  display.setTextColor(SSD1306_WHITE);

  showMessage("Booting Core...");

  loadConfig();
  setupWebServer();
  connectToWiFi();

  randomSeed(analogRead(A0));
  lastInteractionTime = millis();
  lastWeatherAttempt  = millis();
  randomMidYawnTime   = random(20000, 40000);
}

// ================================================================
//  LOOP
// ================================================================
void loop() {
  server.handleClient();
  MDNS.update();

  unsigned long now = millis();

  if (isApModeActive && WiFi.status() != WL_CONNECTED) {
    showSetupScreen();
    delay(50);
    return;
  }

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

  // Watchdog
  if (!isApModeActive && WiFi.status() != WL_CONNECTED && (now - lastWeatherAttempt > 15000)) {
    WiFi.reconnect();
    lastWeatherAttempt = now;
  }

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
//  EEPROM STORAGE
// ================================================================
void loadConfig() {
  EEPROM.begin(sizeof(BotConfig));
  EEPROM.get(0, botConfig);

  if (botConfig.magic == EEPROM_MAGIC) {
    Serial.println(F("Loaded config from EEPROM:"));
    Serial.print(F("SSID: "));      Serial.println(botConfig.ssid);
    Serial.print(F("Location: "));  Serial.println(botConfig.placeName);
    Serial.print(F("Lat/Lon: "));   Serial.print(botConfig.latitude);
    Serial.print(F(", "));          Serial.println(botConfig.longitude);
  } else {
    Serial.println(F("Initializing default config..."));
    botConfig.magic = EEPROM_MAGIC;
    botConfig.ssid[0] = '\0';
    botConfig.password[0] = '\0';
    strncpy(botConfig.placeName, "RANCHI", sizeof(botConfig.placeName));
    botConfig.latitude  = 23.3441;
    botConfig.longitude = 85.3096;
    saveConfig();
  }
}

void saveConfig() {
  EEPROM.put(0, botConfig);
  EEPROM.commit();
  Serial.println(F("Config committed to EEPROM."));
}

void resetConfig() {
  botConfig.magic = 0xFF;
  EEPROM.put(0, botConfig);
  EEPROM.commit();
  Serial.println(F("EEPROM credentials wiped."));
}

// ================================================================
//  PANTHEON AI GATEWAY DASHBOARD
// ================================================================
String buildPortalPage() {
  int n = WiFi.scanNetworks();
  String wifiOptions = "";
  if (n == 0) {
    wifiOptions += "<option disabled>No wireless networks detected</option>";
  } else {
    for (int i = 0; i < n; ++i) {
      int rssi = WiFi.RSSI(i);
      String strength = (rssi >= -60) ? "99% [EXCELLENT]" : (rssi >= -72) ? "75% [GOOD]" : "40% [FAIR]";
      String lock = (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "OPEN" : "WPA2";
      wifiOptions += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " &nbsp;[" + lock + " &bull; " + strength + "]</option>";
    }
  }

  bool isConnected = (WiFi.status() == WL_CONNECTED);
  String ipAddr = isConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  String tempDisplay = weatherReady ? (String(outdoorTemp, 1) + "&deg;C") : "SYNCING...";
  String networkState = isConnected ? "ONLINE // ROUTED" : "AP FALLBACK // STANDBY";
  String nodeStatusColor = isConnected ? "#10b981" : "#f59e0b";

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
  <title>Pantheon Gateway &bull; EmoBot Node</title>
  <style>
    :root {
      --bg: #090a0f;
      --surface: #11131a;
      --surface-elevated: #161922;
      --border: rgba(255, 255, 255, 0.08);
      --border-focus: #6366f1;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --text-dim: #64748b;
      --accent: #6366f1;
      --accent-gradient: linear-gradient(135deg, #6366f1 0%, #4f46e5 100%);
      --cyan: #06b6d4;
      --font-mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background-color: var(--bg);
      background-image: 
        radial-gradient(circle at 50% 0%, rgba(99, 102, 241, 0.15) 0%, transparent 60%),
        radial-gradient(circle at 100% 100%, rgba(6, 182, 212, 0.06) 0%, transparent 40%);
      color: var(--text-main);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 24px 16px;
    }
    .gateway-frame {
      width: 100%;
      max-width: 480px;
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 18px;
      padding: 30px 26px;
      box-shadow: 0 24px 60px -12px rgba(0, 0, 0, 0.8);
      position: relative;
    }
    .gateway-frame::before {
      content: "";
      position: absolute;
      top: 0; left: 0; right: 0; height: 1px;
      background: linear-gradient(90deg, transparent, rgba(99, 102, 241, 0.8), transparent);
    }
    .top-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 24px;
      padding-bottom: 16px;
      border-bottom: 1px solid var(--border);
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .brand-icon {
      width: 28px;
      height: 28px;
      background: var(--surface-elevated);
      border: 1px solid var(--border);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 14px;
    }
    .brand-title {
      font-size: 0.95rem;
      font-weight: 700;
      color: #fff;
    }
    .chip {
      font-family: var(--font-mono);
      font-size: 0.72rem;
      font-weight: 600;
      padding: 4px 10px;
      border-radius: 9999px;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border);
      color: var(--text-muted);
      display: flex;
      align-items: center;
      gap: 6px;
    }
    .pulse-dot {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: )rawliteral" + nodeStatusColor + R"rawliteral(;
      box-shadow: 0 0 8px )rawliteral" + nodeStatusColor + R"rawliteral(;
    }
    .telemetry-grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 8px;
      margin-bottom: 24px;
    }
    .telemetry-card {
      background: var(--surface-elevated);
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 10px 14px;
    }
    .telemetry-card .label {
      font-family: var(--font-mono);
      font-size: 0.68rem;
      color: var(--text-dim);
      text-transform: uppercase;
      margin-bottom: 2px;
    }
    .telemetry-card .val {
      font-family: var(--font-mono);
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-main);
    }
    .section-card {
      background: rgba(255, 255, 255, 0.015);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 18px 16px;
      margin-bottom: 18px;
    }
    .section-title {
      font-size: 0.82rem;
      font-weight: 700;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      color: var(--text-muted);
      margin-bottom: 14px;
    }
    .form-group { margin-bottom: 12px; }
    label {
      display: block;
      font-size: 0.76rem;
      font-weight: 600;
      color: var(--text-dim);
      margin-bottom: 6px;
    }
    input[type="text"], input[type="password"], input[type="number"], select {
      width: 100%;
      height: 42px;
      background: #0b0d14;
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text-main);
      font-size: 0.88rem;
      padding: 0 12px;
      outline: none;
      transition: 0.15s;
      font-family: inherit;
    }
    input:focus, select:focus {
      border-color: var(--border-focus);
      box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.18);
    }
    .row { display: flex; gap: 10px; }
    .col { flex: 1; }
    .btn-secondary {
      background: var(--surface-elevated);
      border: 1px solid var(--border);
      color: var(--text-muted);
      border-radius: 8px;
      height: 34px;
      font-size: 0.78rem;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      width: 100%;
      margin-top: 6px;
    }
    .btn-gps {
      color: var(--cyan);
      border-color: rgba(6, 182, 212, 0.3);
      background: rgba(6, 182, 212, 0.05);
    }
    .btn-submit {
      width: 100%;
      height: 46px;
      background: var(--accent-gradient);
      border: none;
      border-radius: 10px;
      color: #fff;
      font-size: 0.92rem;
      font-weight: 700;
      cursor: pointer;
      box-shadow: 0 4px 18px rgba(99, 102, 241, 0.35);
      margin-top: 8px;
    }
    .password-toggle {
      display: flex;
      align-items: center;
      gap: 6px;
      margin-top: 6px;
      font-size: 0.75rem;
      color: var(--text-dim);
    }
    .footer-stamp {
      text-align: center;
      margin-top: 20px;
      font-family: var(--font-mono);
      font-size: 0.68rem;
      color: var(--text-dim);
    }
  </style>
</head>
<body>
  <div class="gateway-frame">
    <div class="top-bar">
      <div class="brand">
        <div class="brand-icon">&#9889;</div>
        <div class="brand-title">PANTHEON GATEWAY</div>
      </div>
      <div class="chip">
        <span class="pulse-dot"></span>
        <span>)rawliteral" + networkState + R"rawliteral(</span>
      </div>
    </div>

    <div class="telemetry-grid">
      <div class="telemetry-card">
        <div class="label">NODE HOST IP</div>
        <div class="val">)rawliteral" + ipAddr + R"rawliteral(</div>
      </div>
      <div class="telemetry-card">
        <div class="label">LIVE WEATHER</div>
        <div class="val">)rawliteral" + tempDisplay + R"rawliteral(</div>
      </div>
    </div>

    <form action="/save" method="POST">
      <div class="section-card">
        <div class="section-title">&#x1F4F6; 01 // WIRELESS ROUTER</div>
        <div class="form-group">
          <label>DETECTED ACCESS POINTS</label>
          <select id="scanned" onchange="document.getElementById('ssid').value = this.value">
            <option value="" selected disabled>-- Select wireless network --</option>
            )rawliteral" + wifiOptions + R"rawliteral(
          </select>
          <button type="button" class="btn-secondary" onclick="location.reload()">&#x21bb; Re-Scan Channels</button>
        </div>

        <div class="form-group">
          <label>SSID / TARGET NETWORK</label>
          <input type="text" id="ssid" name="ssid" placeholder="Network SSID" value=")rawliteral" + String(botConfig.ssid) + R"rawliteral(" required>
        </div>

        <div class="form-group">
          <label>SECURITY PASSPHRASE</label>
          <input type="password" id="pass" name="password" placeholder="Passphrase" value=")rawliteral" + String(botConfig.password) + R"rawliteral(">
          <div class="password-toggle">
            <input type="checkbox" id="showPass" onclick="var p=document.getElementById('pass'); p.type = p.type==='password'?'text':'password';">
            <label for="showPass" style="margin:0; cursor:pointer;">Display password</label>
          </div>
        </div>
      </div>

      <div class="section-card">
        <div class="section-title">&#x1F310; 02 // WEATHER &amp; TELEMETRY</div>
        <div class="form-group">
          <label>GEOGRAPHIC PRESETS (INCLUDING RANCHI)</label>
          <select id="cityPreset" onchange="applyPreset(this.value)">
            <option value="" selected disabled>-- Quick-Select Location --</option>
            <optgroup label="Jharkhand &amp; Bihar Region">
              <option value="RANCHI,23.3441,85.3096">Ranchi (Jharkhand)</option>
              <option value="JAMSHEDPUR,22.8046,86.2029">Jamshedpur (Jharkhand)</option>
              <option value="DHANBAD,23.7957,86.4304">Dhanbad (Jharkhand)</option>
              <option value="BOKARO,23.6693,86.1511">Bokaro Steel City</option>
              <option value="GAYA JI,24.7914,85.0002">Gaya Ji (Bihar)</option>
              <option value="PATNA,25.6127,85.1589">Patna (Bihar)</option>
            </optgroup>
            <optgroup label="India Metros">
              <option value="NEW DELHI,28.6139,77.2090">New Delhi</option>
              <option value="MUMBAI,19.0760,72.8777">Mumbai</option>
              <option value="BENGALURU,12.9716,77.5946">Bengaluru</option>
              <option value="KOLKATA,22.5726,88.3639">Kolkata</option>
              <option value="HYDERABAD,17.3850,78.4867">Hyderabad</option>
              <option value="PUNE,18.5204,73.8567">Pune</option>
              <option value="CHENNAI,13.0827,80.2707">Chennai</option>
            </optgroup>
            <optgroup label="Global">
              <option value="LONDON,51.5074,-0.1278">London (UK)</option>
              <option value="NEW YORK,40.7128,-74.0060">New York (USA)</option>
              <option value="TOKYO,35.6762,139.6503">Tokyo (Japan)</option>
              <option value="DUBAI,25.2048,55.2708">Dubai (UAE)</option>
              <option value="SINGAPORE,1.3521,103.8198">Singapore</option>
            </optgroup>
          </select>
          <button type="button" class="btn-secondary btn-gps" onclick="getLocation()">&#x2316; Sync Current Device GPS Location</button>
        </div>

        <div class="form-group">
          <label>OLED DISPLAY LABEL (MAX 16 CHARS)</label>
          <input type="text" id="place" name="place" maxlength="16" placeholder="e.g. RANCHI" value=")rawliteral" + String(botConfig.placeName) + R"rawliteral(" required>
        </div>

        <div class="row">
          <div class="col form-group">
            <label>LATITUDE</label>
            <input type="number" step="0.0001" id="lat" name="lat" value=")rawliteral" + String(botConfig.latitude, 4) + R"rawliteral(" required>
          </div>
          <div class="col form-group">
            <label>LONGITUDE</label>
            <input type="number" step="0.0001" id="lon" name="lon" value=")rawliteral" + String(botConfig.longitude, 4) + R"rawliteral(" required>
          </div>
        </div>
      </div>

      <button type="submit" class="btn-submit">&#x2714; DEPLOY CONFIGURATION</button>
    </form>

    <div class="footer-stamp">
      EMOBOT ENGINE &bull; D1 MINI &bull; http://emobot.local
    </div>
  </div>

  <script>
    function applyPreset(val) {
      if(!val) return;
      var p = val.split(',');
      document.getElementById('place').value = p[0];
      document.getElementById('lat').value = p[1];
      document.getElementById('lon').value = p[2];
    }

    function getLocation() {
      if (!navigator.geolocation) {
        alert("GPS not supported by browser.");
        return;
      }
      navigator.geolocation.getCurrentPosition(
        function(pos) {
          document.getElementById('lat').value = pos.coords.latitude.toFixed(4);
          document.getElementById('lon').value = pos.coords.longitude.toFixed(4);
          if(!document.getElementById('place').value || document.getElementById('place').value === "GAYA JI") {
            document.getElementById('place').value = "RANCHI";
          }
          alert("GPS Coordinates synchronized successfully!");
        },
        function(err) {
          alert("GPS Error: " + err.message);
        }
      );
    }
  </script>
</body>
</html>
)rawliteral";
  return html;
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", buildPortalPage());
  });

  server.on("/save", HTTP_POST, []() {
    String newSsid  = server.arg("ssid");
    String newPass  = server.arg("password");
    String newPlace = server.arg("place");
    String newLat   = server.arg("lat");
    String newLon   = server.arg("lon");

    newSsid.trim();
    newPass.trim();
    newPlace.trim();

    if (newSsid.length() > 0) {
      strncpy(botConfig.ssid, newSsid.c_str(), sizeof(botConfig.ssid) - 1);
      botConfig.ssid[sizeof(botConfig.ssid) - 1] = '\0';

      strncpy(botConfig.password, newPass.c_str(), sizeof(botConfig.password) - 1);
      botConfig.password[sizeof(botConfig.password) - 1] = '\0';

      if (newPlace.length() > 0) {
        strncpy(botConfig.placeName, newPlace.c_str(), sizeof(botConfig.placeName) - 1);
        botConfig.placeName[sizeof(botConfig.placeName) - 1] = '\0';
      }

      if (newLat.length() > 0) botConfig.latitude  = newLat.toFloat();
      if (newLon.length() > 0) botConfig.longitude = newLon.toFloat();

      saveConfig();

      String res = "<!DOCTYPE html><html><body style='background:#090a0f;color:#fff;font-family:sans-serif;text-align:center;padding:50px;'>"
                   "<h2>&#10004; Configuration Applied</h2><p>Rebooting gateway node...</p></body></html>";
      server.send(200, "text/html", res);

      delay(1500);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Error: Target SSID must be provided!");
    }
  });

  server.on("/reset", HTTP_GET, []() {
    resetConfig();
    server.send(200, "text/plain", "Credentials wiped. Restarting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println(F("Gateway Web Server active."));

  if (MDNS.begin("emobot")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("mDNS responder registered: http://emobot.local"));
  }
}

// ================================================================
//  WIFI AP & CONNECTION HANDLER
// ================================================================
void startAccessPoint() {
  isApModeActive = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println(F("\n[!] Initialized Gateway Hotspot:"));
  Serial.print(F("SSID: ")); Serial.println(AP_SSID);
  Serial.print(F("PASS: ")); Serial.println(AP_PASS);
  Serial.print(F("IP: "));   Serial.println(WiFi.softAPIP());

  showSetupScreen();
}

void showSetupScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 4);
  display.println(F("PANTHEON GATEWAY"));
  display.drawFastHLine(0, 15, SCREEN_WIDTH, WHITE);
  display.setCursor(0, 20);
  display.print(F("AP: ")); display.println(AP_SSID);
  display.setCursor(0, 32);
  display.print(F("PW: ")); display.println(AP_PASS);
  display.setCursor(0, 44);
  display.print(F("IP: ")); display.println(WiFi.softAPIP());
  display.setCursor(0, 56);
  display.println(F("http://emobot.local"));
  display.display();
}

void connectToWiFi() {
  if (strlen(botConfig.ssid) == 0) {
    Serial.println(F("Zero-config detected. Launching Gateway AP."));
    startAccessPoint();
    return;
  }

  showMessage("Connecting WiFi");
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(botConfig.ssid, botConfig.password);
  Serial.print(F("Connecting to "));
  Serial.print(botConfig.ssid);

  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 12000) {
    delay(500);
    Serial.print('.');
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi Connected!"));
    Serial.print(F("IP: ")); Serial.println(WiFi.localIP());
    isApModeActive = false;
    WiFi.mode(WIFI_STA);
    syncTime();
    fetchWeather();
  } else {
    Serial.println(F("\nConnection timed out. Triggering fallback AP..."));
    startAccessPoint();
  }
}

// ================================================================
//  WEATHER
// ================================================================
void fetchWeather() {
  Serial.println(F("Fetching weather telemetry..."));
  weatherFetchFailed = false;

  WiFiClient client;
  HTTPClient http;

  String url = "http://wttr.in/" +
               String(botConfig.latitude, 4) + "," + String(botConfig.longitude, 4) +
               "?format=%t&m";

  http.setTimeout(8000);

  if (!http.begin(client, url)) {
    weatherFetchFailed = true;
    Serial.println(F("HTTP begin failed"));
    return;
  }

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  Serial.print(F("HTTP response: ")); Serial.println(code);

  if (code == 200) {
    String payload = http.getString();
    http.end();
    payload.trim();

    if (payload.length() > 0 && payload[0] == '+') payload.remove(0, 1);

    int cutAt = payload.length();
    for (int i = 0; i < (int)payload.length(); i++) {
      byte c = (byte)payload[i];
      if (c > 127 || c == 'C') { cutAt = i; break; }
    }
    String numStr = payload.substring(0, cutAt);
    numStr.trim();

    float parsed = numStr.toFloat();
    if (numStr.length() > 0 && parsed > -50.0f && parsed < 65.0f) {
      outdoorTemp        = parsed;
      weatherReady       = true;
      weatherFetchFailed = false;
      lastWeatherUpdate  = millis();
      Serial.print(F("Telemetry OK: ")); Serial.print(outdoorTemp); Serial.println(F(" C"));
    } else {
      weatherFetchFailed = true;
    }
  } else {
    http.end();
    weatherFetchFailed = true;
  }
}

void showWeather() {
  if (!weatherReady) {
    if (WiFi.status() != WL_CONNECTED) {
      centerText("No WiFi!",      10, 1);
      centerText("Open Gateway:", 24, 1);
      centerText("192.168.4.1",   36, 1);
      centerText("to configure",  48, 1);
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

  centerText(botConfig.placeName, 0, 1);
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
      centerText("Open Gateway:",    30, 1);
      centerText("192.168.4.1",      42, 1);
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
