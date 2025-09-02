#define BLYNK_TEMPLATE_ID "TMPL3oGGrYApx"
#define BLYNK_TEMPLATE_NAME "Smart Water Bottle"
#define BLYNK_AUTH_TOKEN "W2TbATehtZiGHpJTX0A2M9T_8txMHP8h"

#include <U8g2lib.h>
#include <Wire.h>
#include <HX711.h>
#include <RTClib.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// WiFi Credentials
char ssid[] = "Airtel_n000_4168";
char pass[] = "air48556";

// NTP Setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // IST offset: +5:30

// OLED Setup
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// HX711 Setup
#define HX711_DOUT 32
#define HX711_SCK 33
HX711 scale;
float calibration_factor = 402.43;
const int samples = 10;

// RTC Setup
RTC_DS3231 rtc;

// Buzzer Setup
#define BUZZER_PIN 5
#define BUZZER_DURATION 3000  // 3 seconds
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;

// Water Intake Tracking
#define DAILY_GOAL 3000
#define MAX_BOTTLE_CAPACITY 1000
#define BOTTLE_TARE 60.0     // empty bottle weight in grams (≈mL)
#define DRINK_THRESHOLD 5.0  // mL threshold to count as a real drink/refill
#define NOISE_MARGIN 3.0     // margin around 0 to kill float noise

float dailyIntake = 0;
float lastPlacedWeight = 0;   // baseline "remaining" when bottle last placed
float lastDrink = 0;          // last drink amount
bool bottleRemoved = false;
bool baselineSet = false;     // true after first placement

// Timer for 1-minute reminders
unsigned long lastReminderTime = 0;
const unsigned long reminderInterval = 60000; // 60s

// Reconnection timers
unsigned long lastConnectAttempt = 0;
const unsigned long connectInterval = 30000; // 30s
unsigned long lastTimeUpdate = 0;
const unsigned long timeUpdateInterval = 3600000; // 1 hour

// ---------------- Helper Functions ----------------

// Read and process current weight from scale
float readRemaining() {
  if (!scale.is_ready()) {
    Serial.println("⚠️ HX711 not ready");
    return -999.0;
  }

  float rawWeight = scale.get_units(samples);
  if (rawWeight < 0) rawWeight = 0;
  if (rawWeight > MAX_BOTTLE_CAPACITY + BOTTLE_TARE + 200) {
    rawWeight = 0; // ignore nonsense spikes
    Serial.println("⚠️ Ignoring weight spike");
  }

  float remaining = rawWeight - BOTTLE_TARE;
  if (remaining < NOISE_MARGIN) remaining = 0; // squash noise near zero
  return remaining;
}

// Detect bottle events and update intake
void detectBottleEvents(float remaining, bool bottlePresent) {
  // First valid placement after boot/reset → just set baseline, do not count as drink/refill
  if (bottlePresent && !baselineSet) {
    lastPlacedWeight = remaining;
    baselineSet = true;
    Serial.print("Baseline set (remaining mL): ");
    Serial.println(lastPlacedWeight, 2);
    return;
  }

  // Bottle lifted
  if (!bottlePresent && baselineSet && !bottleRemoved) {
    bottleRemoved = true;
    Serial.println("Bottle lifted...");
  }

  // Bottle placed back
  if (bottlePresent && bottleRemoved) {
    float afterLiftWeight = remaining;
    float delta = lastPlacedWeight - afterLiftWeight;

    if (delta > DRINK_THRESHOLD) {
      dailyIntake += delta;
      lastDrink = delta;
      Serial.print("Drink detected: ");
      Serial.print(delta, 2);
      Serial.println(" mL");
    } else if (afterLiftWeight > lastPlacedWeight + DRINK_THRESHOLD) {
      float refillAmount = afterLiftWeight - lastPlacedWeight;
      Serial.print("Refill detected: +");
      Serial.print(refillAmount, 2);
      Serial.println(" mL");
    }

    lastPlacedWeight = afterLiftWeight;
    bottleRemoved = false;
  }
}

// Update Blynk if connected
void updateBlynk(float remaining) {
  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, remaining);
    Blynk.virtualWrite(V1, dailyIntake);
    Blynk.virtualWrite(V2, lastDrink);
  }
}

// Handle daily reset at midnight
void handleDailyReset(DateTime now) {
  if (now.hour() == 0 && now.minute() == 0 && now.second() < 2) {
    dailyIntake = 0;
    lastDrink = 0;
    baselineSet = false;
    lastPlacedWeight = 0;
    updateBlynk(0); // Push reset to Blynk
    Serial.println("Daily intake reset.");
  }
}

// Handle water reminder
void handleReminder() {
  if (millis() - lastReminderTime >= reminderInterval) {
    lastReminderTime = millis();
    if (Blynk.connected()) {
      Blynk.logEvent("water_reminder", "Time to drink water!");
    }
    // Start buzzer non-blockingly
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerStartTime = millis();
    Serial.println("Reminder triggered.");
  }

  // Check if buzzer duration is over
  if (buzzerActive && millis() - buzzerStartTime >= BUZZER_DURATION) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

// Update OLED display
void updateDisplay(DateTime now, float remaining, bool bottlePresent) {
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  char dateStr[11];
  sprintf(dateStr, "%02d/%02d/%04d", now.day(), now.month(), now.year());

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.setCursor(2, 10); u8g2.print("Time: "); u8g2.print(timeStr);
  u8g2.setCursor(2, 22); u8g2.print("Date: "); u8g2.print(dateStr);

  u8g2.setCursor(2, 34);
  if (!bottlePresent) {
    u8g2.print("Remaining: No Bottle");
  } else {
    u8g2.printf("Remaining: %.2f mL", remaining);
  }

  u8g2.setCursor(2, 46);
  u8g2.printf("Today's Intake: %.2f mL", dailyIntake);

  u8g2.setCursor(2, 58);
  u8g2.printf("Last Intake: %.2f mL", lastDrink);

  u8g2.sendBuffer();
}

// Handle WiFi and Blynk reconnection
void handleConnections() {
  if (millis() - lastConnectAttempt >= connectInterval) {
    lastConnectAttempt = millis();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Attempting WiFi reconnection...");
      WiFi.begin(ssid, pass);
      int wifiAttempts = 0;
      while (WiFi.status() != WL_CONNECTED && wifiAttempts < 10) {
        delay(500);
        wifiAttempts++;
      }
    }

    if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
      Serial.println("Attempting Blynk reconnection...");
      Blynk.connect(10000);
    }
  }
}

// Update RTC from NTP if needed
void updateTimeFromNTP() {
  if (WiFi.status() == WL_CONNECTED && millis() - lastTimeUpdate >= timeUpdateInterval) {
    lastTimeUpdate = millis();
    if (timeClient.update()) {
      rtc.adjust(DateTime(timeClient.getEpochTime()));
      Serial.println("RTC updated from NTP.");
    }
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting Smart Water Bottle...");

  Wire.begin(21, 22);
  Wire.setClock(100000);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(2, 30, "Initializing...");
  u8g2.sendBuffer();

  // HX711
  scale.begin(HX711_DOUT, HX711_SCK);
  if (scale.is_ready()) {
    scale.set_scale(calibration_factor);
    scale.tare();
    Serial.println("HX711 initialized.");
  } else {
    Serial.println("⚠️ HX711 not ready at startup");
  }

  // RTC
  if (!rtc.begin()) {
    Serial.println("❌ RTC not found!");
    while (1) delay(10);
  }

  // WiFi + NTP + Blynk
  WiFi.begin(ssid, pass);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 15) {
    delay(500);
    wifiAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    timeClient.begin();
    if (timeClient.update()) {
      rtc.adjust(DateTime(timeClient.getEpochTime()));
      Serial.println("RTC set from NTP.");
    }
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect(10000);
  } else {
    Serial.println("WiFi connection failed.");
  }

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("System Ready...");
  u8g2.clearBuffer();
  u8g2.drawStr(2, 30, "System Ready");
  u8g2.sendBuffer();
  delay(1500);
}

// ---------------- LOOP ----------------
void loop() {
  // Handle connections and Blynk run
  handleConnections();
  if (WiFi.status() == WL_CONNECTED && Blynk.connected()) {
    Blynk.run();
  }

  // Update time from NTP periodically
  updateTimeFromNTP();

  // Get current time
  DateTime now = rtc.now();

  // Read weight and determine presence
  float remaining = readRemaining();
  bool bottlePresent = (remaining >= (0 - NOISE_MARGIN)); // Use remaining after adjustment

  // Detect events if valid reading
  if (remaining != -999.0) {
    detectBottleEvents(remaining, bottlePresent);
    updateBlynk(remaining);
  }

  // Handle reset and reminder
  handleDailyReset(now);
  handleReminder();

  // Update display
  updateDisplay(now, remaining, bottlePresent);

  delay(500);
}