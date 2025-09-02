Smart Water Bottle
The Smart Water Bottle is an IoT-enabled project that tracks water intake using an ESP32 microcontroller, an HX711 load cell amplifier, a DS3231 RTC, an OLED display, and a buzzer for reminders. It connects to the Blynk platform to monitor water consumption in real-time via a mobile app.

Table of Contents

Project Overview
Features
Hardware Requirements
Circuit Diagram
Software Requirements
Setup Instructions
Usage
Troubleshooting
License

Project Overview
This project monitors the water level in a bottle using a load cell and HX711 amplifier, displays the time, date, and water intake on an OLED screen, and sends data to the Blynk app. A buzzer provides periodic reminders to drink water, and the DS3231 RTC ensures accurate timekeeping. The system resets daily intake at midnight and tracks drinking events by detecting bottle lifts and placements.
Features

Measures water weight using an HX711 load cell amplifier.
Displays time, date, remaining water, daily intake, and last drink on a 128x64 OLED.
Sends data to Blynk app for remote monitoring (Virtual Pins V0, V1, V2).
Buzzer reminders every 60 seconds.
Daily intake reset at midnight using DS3231 RTC.
WiFi connectivity for NTP time sync and Blynk integration.

Hardware Requirements

ESP32 Dev Module (e.g., ESP32-WROOM-32)
HX711 Load Cell Amplifier with a load cell (e.g., 1kg or 5kg)
SSD1306 128x64 OLED Display (I2C interface)
DS3231 RTC Module
Buzzer (active or passive)
Power Supply: 5V/1A+ USB or battery (7.4V LiPo with regulator)
Jumper wires, breadboard, or custom PCB
A water bottle (calibrated empty weight: ~60g)

Circuit Diagram

Connections:

HX711:
DOUT: ESP32 Pin 32
SCK: ESP32 Pin 33
VCC: 3.3V or 5V
GND: ESP32 GND


Load Cell: E+ (red), E- (black), A+ (green), A- (white) to HX711
OLED (I2C):
SDA: ESP32 Pin 21
SCL: ESP32 Pin 22
VCC: 3.3V
GND: ESP32 GND


DS3231 RTC (I2C):
SDA: ESP32 Pin 21
SCL: ESP32 Pin 22
VCC: 3.3V or 5V
GND: ESP32 GND


Buzzer:
Positive: ESP32 Pin 5
Negative: ESP32 GND



Software Requirements

Arduino IDE (2.x or later)
ESP32 Arduino Core: Install via Boards Manager
Libraries (listed in libraries.txt):
U8g2 (for OLED)
HX711 (for load cell)
RTClib (for RTC)
BlynkSimpleEsp32 (for Blynk)
WiFi and WiFiUdp (included with ESP32 core)
NTPClient (for time sync)


Blynk App: Set up with template ID TMPL3oGGrYApx and auth token.

Install libraries via Arduino IDE: Sketch > Include Library > Manage Libraries.
Setup Instructions

Hardware Setup:

Connect components as per the circuit diagram.
Secure the load cell to measure the bottle’s weight (e.g., bottle placed on load cell platform).
Ensure stable power (5V/1A+ USB or battery).


Software Setup:

Install Arduino IDE and ESP32 core.
Install libraries listed in libraries.txt.
Open src/SmartWaterBottle.ino in Arduino IDE.
Update WiFi credentials (ssid, pass) in the code.
Set calibration_factor (default: 402.43) after calibrating the HX711 (see Troubleshooting).


Blynk Setup:

Download the Blynk app (iOS/Android).
Create a new project with template ID TMPL3oGGrYApx.
Copy the auth token into BLYNK_AUTH_TOKEN.
Add datastreams:
V0: Remaining water (mL)
V1: Daily intake (mL)
V2: Last drink (mL)
Event: water_reminder




Upload Code:

Select ESP32 Dev Module in Arduino IDE.
Upload SmartWaterBottle.ino to the ESP32.
Open Serial Monitor (115200 baud) to verify initialization.



Usage

Power On: The OLED displays “System Ready” after initialization.
Place Bottle: The system detects the bottle (weight ≥ 60g) and sets a baseline.
Drink Tracking: Lift and replace the bottle to record drinks (≥ 5mL).
Blynk App: Monitor remaining water (V0), daily intake (V1), and last drink (V2).
Reminders: Buzzer beeps every 60 seconds; Blynk sends “Time to drink water!” notifications.
Daily Reset: Intake resets at midnight (RTC-based).

Troubleshooting

HX711 Issues (❌ HX711 not ready, large negative weights):
Check wiring: DOUT (Pin 32), SCK (Pin 33), VCC (3.3V/5V), GND.
Swap load cell A+/A- wires if readings are negative.
Calibrate HX711:
Use the calibration sketch from HX711 Calibration.
Remove weight, type t to tare, place a known weight (e.g., 100g), adjust calibration_factor with +/-.
Update calibration_factor in SmartWaterBottle.ino.


Try pins 18 (DOUT) and 19 (SCK) if 32/33 fail.
Measure VCC voltage (3.3–5V) and ensure stable power.


WiFi/Blynk Failure:
Verify ssid, pass, and BLYNK_AUTH_TOKEN.
Ensure ESP32 is within WiFi range.


RTC Issues:
Check I2C wiring (SDA=21, SCL=22).
Replace RTC battery if time is incorrect.



HX711 Calibration
Use this sketch to calibrate the HX711 (save as src/HX711_Calibration.ino):
#include <HX711.h>
#define HX711_DOUT 32
#define HX711_SCK 33
HX711 scale;
float calibration_factor = 402.43;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Testing HX711...");
  pinMode(HX711_DOUT, INPUT);
  pinMode(HX711_SCK, OUTPUT);
  digitalWrite(HX711_SCK, LOW);
  Serial.print("DOUT pin state: ");
  Serial.println(digitalRead(HX711_DOUT));
  scale.begin(HX711_DOUT, HX711_SCK);
  int retryCount = 0;
  const int maxRetries = 10;
  while (!scale.is_ready() && retryCount < maxRetries) {
    Serial.printf("⚠️ HX711 not ready, retry %d/%d\n", retryCount + 1, maxRetries);
    delay(1000);
    scale.begin(HX711_DOUT, HX711_SCK);
    retryCount++;
  }
  if (scale.is_ready()) {
    Serial.println("✅ HX711 ready");
  } else {
    Serial.println("❌ HX711 not ready, proceeding anyway");
  }
  scale.set_scale(calibration_factor);
  Serial.println("Remove all weight and press 't' to tare");
  while (!Serial.available()) delay(10);
  if (Serial.read() == 't') {
    scale.tare();
    Serial.println("Tare done");
  }
  Serial.println("Place a known weight (e.g., 100g) and enter its weight in grams:");
}

void loop() {
  float raw = scale.read();
  float weight = scale.get_units(10);
  Serial.print("Raw reading: ");
  Serial.println(raw);
  Serial.print("Weight: ");
  Serial.println(weight, 2);
  Serial.print("DOUT pin state: ");
  Serial.println(digitalRead(HX711_DOUT));
  if (Serial.available()) {
    char input = Serial.read();
    if (input == '+') {
      calibration_factor += 10;
      scale.set_scale(calibration_factor);
      Serial.print("New calibration factor: ");
      Serial.println(calibration_factor);
    } else if (input == '-') {
      calibration_factor -= 10;
      scale.set_scale(calibration_factor);
      Serial.print("New calibration factor: ");
      Serial.println(calibration_factor);
    } else if (input == 't') {
      scale.tare();
      Serial.println("Tare done");
    }
  }
  delay(1000);
}

Video Demonstration
Watch the prototype in action: Smart Water Bottle Demo
License
This project is licensed under the MIT License - see the LICENSE file for details.
