#define BLYNK_TEMPLATE_ID "TMPL3oGGrYApx"
#define BLYNK_TEMPLATE_NAME "Smart Water Bottle"
#define BLYNK_AUTH_TOKEN "W2TbATehtZiGHpJTX0A2M9T_8txMHP8h"

#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <HX711.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <BlynkSimpleEsp32.h>

// OLED Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#define OLED_FONT u8g2_font_ncenB08_tr

// RTC
RTC_DS3231 rtc;

// HX711 Setup
#define HX711_DOUT 32
#define HX711_SCK 33
HX711 scale;
const float CALIBRATION_FACTOR = 198.39f; // Adjust after calibration
constexpr uint8_t SAMPLES = 10; // Reduced for faster readings
constexpr uint8_t SAMPLES_PRESENCE = 2; // Reduced for faster presence detection
const float EMA_ALPHA = 0.6f; // Increased for faster response to new weights
bool useRawForInitial = true;
uint8_t initialReadCount = 0;
const uint8_t INITIAL_READ_BURST = 3; // Reduced for faster initial stabilization
float emaWeight = 0.0f;

// Buzzer Setup
constexpr uint8_t BUZZER_PIN = 5;
constexpr unsigned long BUZZER_DURATION_MS = 3000;
bool buzzerOn = false;
unsigned long buzzerStart = 0;

// NTP Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);
unsigned long lastSync = 0;
constexpr unsigned long SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;

// Thresholds and timings
constexpr float BOTTLE_PRESENT_THRESHOLD = 5.0f; // Increased to ignore noise and avoid 0ml toggle
constexpr float DRINK_THRESHOLD = 1.0f;
constexpr float REFILL_THRESHOLD = 2.0f;
constexpr float NOISE_MARGIN = 2.0f;
constexpr float DAILY_GOAL = 3000.0f;

constexpr unsigned long BOTTLE_EVENT_DEBOUNCE_MS = 500; // Reduced for faster state transitions
constexpr unsigned long NOTIFICATION_INTERVAL_MS = 60000; // 1 minute
constexpr unsigned long BLYNK_UPDATE_INTERVAL_MS = 5000;

float dailyIntake = 0.0f;
float lastDrink = 0.0f;
float onStandWeight = 0.0f;
float preLiftWeight = 0.0f;
float remaining = 0.0f;

enum BottleState { OFF_STAND, ON_STAND };
BottleState state = OFF_STAND;

int lastResetY = -1, lastResetM = -1, lastResetD = -1;

unsigned long lastBottleEventTime = 0;
unsigned long lastNotificationTime = 0;
unsigned long lastBlynkUpdate = 0;

WiFiManager wm;

float readStableWeight() {
  if (!scale.is_ready()) {
    Serial.println("HX711 not ready - check wiring or power.");
    return -999.0f;
  }
  float raw = scale.get_units(SAMPLES);
  if (raw < 0) raw = 0;
  Serial.printf("Raw weight: %.2f\n", raw);
  if (useRawForInitial && initialReadCount < INITIAL_READ_BURST) {
    initialReadCount++;
    Serial.printf("Initial raw weight #%d: %.2f\n", initialReadCount, raw);
    return raw;
  }
  if (emaWeight == 0.0f) emaWeight = raw;
  else emaWeight = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * emaWeight;
  Serial.printf("EMA weight: %.2f\n", emaWeight);
  return emaWeight;
}

bool bottleIsPresent(float w) {
  return (w > BOTTLE_PRESENT_THRESHOLD) && (w != -999.0f);
}

void displayOLED(const DateTime& now, bool present, float rem, float today, float last) {
  char dateStr[20], timeStr[20], line[32];
  snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", now.day(), now.month(), now.year());
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);

  u8g2.setCursor(2, 12);
  u8g2.print("Date: ");
  u8g2.print(dateStr);

  u8g2.setCursor(2, 24);
  u8g2.print("Time: ");
  u8g2.print(timeStr);

  u8g2.setCursor(2, 36);
  if (!present || rem < BOTTLE_PRESENT_THRESHOLD) u8g2.print("Remaining: No Bottle");
  else {
    snprintf(line, sizeof(line), "Remaining: %.0f mL", rem);
    u8g2.print(line);
  }
  
  u8g2.setCursor(2, 48);
  snprintf(line, sizeof(line), "Today's Intake: %.0f mL", today);
  u8g2.print(line);

  u8g2.setCursor(2, 60);
  snprintf(line, sizeof(line), "Last Intake: %.0f mL", last);
  u8g2.print(line);

  u8g2.sendBuffer();
}

void safeMidnightReset(const DateTime& now) {
  if (now.year() != lastResetY || now.month() != lastResetM || now.day() != lastResetD) {
    dailyIntake = 0.0f;
    lastDrink = 0.0f;
    lastResetY = now.year();
    lastResetM = now.month();
    lastResetD = now.day();
    Serial.println("[Reset] New day, counters cleared.");
    emaWeight = 0.0f;
    useRawForInitial = true;
    initialReadCount = 0;
  }
}

void buzzerHandler() {
  if (buzzerOn && (millis() - buzzerStart >= BUZZER_DURATION_MS)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Smart Water Bottle Initializing...");

  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(CALIBRATION_FACTOR);
  delay(200); // Reduced delay for faster setup

  // Calibration debug: Place a known weight (e.g., 100g) and check raw reading
  float raw = scale.read_average(10);
  Serial.printf("Calibration: Raw reading for known weight (e.g., 100g): %f\n", raw);

  scale.tare(); // Force tare for reliability
  Serial.println("Forced tare in setup.");

  dailyIntake = 0.0f; // Clear stale data
  lastDrink = 0.0f;   // Clear stale data

  Wire.begin();
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(OLED_FONT);
  u8g2.drawStr(2, 34, "Initializing...");
  u8g2.sendBuffer();

  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
    u8g2.clearBuffer();
    u8g2.drawStr(2, 34, "RTC Error!");
    u8g2.sendBuffer();
    while (1) delay(10);
  }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Initialize WiFiManager with a timeout
  wm.setConfigPortalTimeout(30); // 30-second timeout
  wm.setAPCallback([](WiFiManager*) {
    Serial.println("Started AP mode for WiFi setup: connect to 'SmartBottleSetup' at 192.168.4.1");
    // No OLED update for WiFi setup
  });

  // Attempt to connect to saved WiFi credentials without blocking
  WiFi.begin(); // Start WiFi with saved credentials
  unsigned long wifiStartTime = millis();
  bool wifiConnected = false;
  while (millis() - wifiStartTime < 5000) { // Reduced to 5 seconds
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("Connected to WiFi");
      break;
    }
    delay(50); // Reduced delay for faster loop
    // Update OLED with water data during WiFi connection attempt
    DateTime now = rtc.now();
    bool scaleError = !scale.is_ready();
    float w = scaleError ? -999.0f : readStableWeight();
    bool present = bottleIsPresent(w);
    displayOLED(now, present && !scaleError, remaining, dailyIntake, lastDrink);
  }

  if (!wifiConnected) {
    Serial.println("WiFi connection failed - starting config portal in background");
    wm.startConfigPortal("SmartBottleSetup", NULL); // Non-blocking AP mode
  }

  // Initialize NTP and Blynk if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    if (timeClient.update()) {
      rtc.adjust(DateTime(timeClient.getEpochTime()));
      lastSync = millis();
      Serial.println("RTC synced from NTP");
    }
    Blynk.config(BLYNK_AUTH_TOKEN);
    if (Blynk.connect(5000)) {
      Serial.println("Blynk connected successfully.");
    } else {
      Serial.println("Blynk initial connection failed - will retry.");
    }
  } else {
    Serial.println("Continuing in offline mode.");
  }

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  DateTime now = rtc.now();
  lastResetY = now.year();
  lastResetM = now.month();
  lastResetD = now.day();
  lastNotificationTime = millis();

  Serial.println("System Ready.");
  // Immediately show water data
  bool scaleError = !scale.is_ready();
  float w = scaleError ? -999.0f : readStableWeight();
  bool present = bottleIsPresent(w);
  displayOLED(now, present && !scaleError, remaining, dailyIntake, lastDrink);
}

void loop() {
  static int wifiFailCount = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!Blynk.connected()) Blynk.connect(5000);
    if (Blynk.connected()) Blynk.run();
  }

  static unsigned long lastReconnectAttempt = 0;
  if (millis() - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      wifiFailCount++;
      Serial.println("WiFi reconnect attempted.");
      if (wifiFailCount > 3) {
        wm.resetSettings();
        Serial.println("WiFi settings reset - starting config portal in background");
        wm.startConfigPortal("SmartBottleSetup", NULL); // Non-blocking
        wifiFailCount = 0;
      }
    } else {
      wifiFailCount = 0;
    }
    Serial.printf("WiFi Status: %s | Blynk Connected: %s\n", 
                  WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected", 
                  Blynk.connected() ? "Yes" : "No");
  }

  if (WiFi.status() == WL_CONNECTED && (millis() - lastSync > SYNC_INTERVAL_MS)) {
    if (timeClient.update()) {
      rtc.adjust(DateTime(timeClient.getEpochTime()));
      lastSync = millis();
      Serial.println("RTC re-synced from NTP.");
    }
  }

  DateTime now = rtc.now();
  safeMidnightReset(now);

  bool scaleError = !scale.is_ready();
  float w;
  bool present;
  static bool last_present = false;
  if (scaleError) {
    Serial.println("HX711 not ready - check wiring or power.");
    w = -999.0f;
    present = false;
  } else {
    float presence_raw = scale.get_units(SAMPLES_PRESENCE);
    if (presence_raw < 0) presence_raw = 0.0f;
    bool quick_present = (presence_raw > BOTTLE_PRESENT_THRESHOLD);
    if (quick_present && !last_present && state == OFF_STAND) {
      useRawForInitial = true;
      initialReadCount = 0;
    }
    last_present = quick_present;
    present = quick_present;
    w = readStableWeight();
  }

  unsigned long nowMillis = millis();

  if (present && state == ON_STAND) {
    remaining = onStandWeight;
  } else if (!present && state == OFF_STAND) {
    remaining = 0.0f;
  } else if (present && state == OFF_STAND && nowMillis - lastBottleEventTime > BOTTLE_EVENT_DEBOUNCE_MS) {
    // Placement event
    float after = scale.get_units(SAMPLES);
    if (after < 0) after = 0.0f;
    if (after > BOTTLE_PRESENT_THRESHOLD) {  // Only process if significant weight
      emaWeight = after;
      if (preLiftWeight == 0.0f) {
        preLiftWeight = after;
        Serial.printf("[Init Place] Baseline: %.1f mL\n", preLiftWeight);
      } else {
        float delta = preLiftWeight - after;
        if (fabs(delta) < NOISE_MARGIN) delta = 0.0f;
        if (delta > DRINK_THRESHOLD) {
          dailyIntake += delta;
          lastDrink = delta;
          Serial.printf("[Drink] %.1f mL (today: %.1f, goal: %.1f%%)\n", delta, dailyIntake, (dailyIntake / DAILY_GOAL) * 100.0f);
        } else if ((after - preLiftWeight) > REFILL_THRESHOLD) {
          float refillAmt = after - preLiftWeight;
          Serial.printf("[Refill] +%.1f mL\n", refillAmt);
        }
      }
      onStandWeight = after;
      remaining = after;
      state = ON_STAND;
      lastBottleEventTime = nowMillis;
    } else {
      // Ignore low weight as noise, treat as no bottle
      present = false;
      remaining = 0.0f;
      Serial.println("[Noise] Low weight ignored as no bottle.");
    }
  } else if (!present && state == ON_STAND && nowMillis - lastBottleEventTime > BOTTLE_EVENT_DEBOUNCE_MS) {
    // Removal event
    preLiftWeight = onStandWeight;
    state = OFF_STAND;
    remaining = 0.0f;
    lastBottleEventTime = nowMillis;
    Serial.printf("[Lift] Pre-lift: %.1f mL\n", preLiftWeight);
  } else {
    // Transient state
    if (present) {
      remaining = w;
    } else {
      remaining = 0.0f;
    }
  }

  if (remaining < NOISE_MARGIN) remaining = 0.0f;

  displayOLED(now, present && !scaleError, remaining, dailyIntake, lastDrink);

  static unsigned long lastBlynkSend = 0;
  if (Blynk.connected() && (nowMillis - lastBlynkSend > BLYNK_UPDATE_INTERVAL_MS)) {
    Blynk.virtualWrite(V0, remaining);
    Blynk.virtualWrite(V1, dailyIntake);
    Blynk.virtualWrite(V2, lastDrink);
    Blynk.virtualWrite(V3, (dailyIntake / DAILY_GOAL) * 100.0f);
    lastBlynkSend = nowMillis;
    Serial.println("Data sent to Blynk.");
  }

  if (nowMillis - lastNotificationTime >= NOTIFICATION_INTERVAL_MS) {
    lastNotificationTime = nowMillis;
    if (Blynk.connected()) {
      Blynk.logEvent("water_reminder", "It's time to drink water! Stay hydrated.");
    }
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOn = true;
    buzzerStart = nowMillis;
    Serial.println("🔔 Reminder: Drink water!");
  }

  buzzerHandler();

  if (!scaleError) {
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d | Weight: %.2f | Remain: %.2f | Daily: %.2f | Last: %.2f | State: %s\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second(),
                  w, remaining, dailyIntake, lastDrink,
                  (state == ON_STAND ? "ON" : "OFF"));
  } else {
    Serial.println("Scale not ready - skipping weight read.");
  }

  delay(100); // Reduced for faster updates
}

BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    Serial.println("Manual reset triggered via Blynk.");
    ESP.restart();
  }
}