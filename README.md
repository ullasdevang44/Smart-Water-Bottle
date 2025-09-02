Smart Water Bottle

The Smart Water Bottle is an IoT-enabled hydration tracker built with an ESP32 microcontroller. It monitors water intake using a load cell and HX711 amplifier, displays data on an OLED screen, tracks time with a DS3231 RTC, and provides real-time monitoring via the Blynk app. A buzzer gives friendly reminders to stay hydrated.

📑 Table of Contents

Overview

Features

Hardware Requirements

Circuit Diagram

Software Requirements

Setup Instructions

Usage

Troubleshooting

HX711 Calibration

License

🔎 Overview

This project tracks daily water intake by detecting weight changes in a bottle.

The HX711 load cell amplifier measures water weight.

A 128x64 OLED shows time, date, water level, and last drink.

Blynk app integration enables remote tracking.

A buzzer provides periodic reminders.

The DS3231 RTC ensures accurate timekeeping, with daily reset at midnight.

✨ Features

✅ Accurate weight measurement with HX711
✅ OLED display (time, date, intake, remaining water, last drink)
✅ Real-time monitoring via Blynk (V0, V1, V2 datastreams)
✅ Buzzer reminders every 60 seconds
✅ Automatic daily intake reset at midnight
✅ WiFi + NTP for time sync and IoT connectivity

🛠 Hardware Requirements

ESP32 Dev Module (e.g., ESP32-WROOM-32)

HX711 Load Cell Amplifier + Load Cell (1kg / 5kg)

128x64 OLED Display (I2C)

DS3231 RTC Module

Buzzer (active or passive)

Power Supply: 5V/1A USB or battery (7.4V LiPo with regulator)

Bottle (calibrated empty weight: ~60g)

Jumper wires, breadboard / PCB

⚡ Circuit Diagram
🔹 HX711

DOUT → ESP32 GPIO 32

SCK → ESP32 GPIO 33

VCC → 3.3V / 5V

GND → ESP32 GND

🔹 Load Cell

E+ (red), E- (black), A+ (green), A- (white) → HX711

🔹 OLED (I2C)

SDA → ESP32 GPIO 21

SCL → ESP32 GPIO 22

VCC → 3.3V

GND → GND

🔹 RTC (DS3231, I2C)

SDA → ESP32 GPIO 21

SCL → ESP32 GPIO 22

VCC → 3.3V / 5V

GND → GND

🔹 Buzzer

+ → ESP32 GPIO 5

- → GND

💻 Software Requirements

Arduino IDE
 (v2.x recommended)

ESP32 Arduino Core (via Boards Manager)

Libraries:

U8g2 (OLED)

HX711 (Load Cell)

RTClib (RTC)

BlynkSimpleEsp32 (Blynk IoT)

WiFi + WiFiUdp (ESP32 Core)

NTPClient (Time Sync)

📱 Blynk App Setup

Template ID: TMPL3oGGrYApx

Datastreams:

V0 → Remaining water (mL)

V1 → Daily intake (mL)

V2 → Last drink (mL)

Event → water_reminder

⚙️ Setup Instructions
1️⃣ Hardware Setup

Connect all components as per the circuit diagram.

Secure the load cell under the bottle.

Ensure stable power (USB 5V / LiPo with regulator).

2️⃣ Software Setup

Install Arduino IDE + ESP32 core.

Install required libraries.

Open SmartWaterBottle.ino.

Update WiFi SSID & Password.

Update Blynk Auth Token.

Set calibration_factor (default: 402.43).

3️⃣ Upload & Test

Select ESP32 Dev Module in Arduino IDE.

Upload sketch → Open Serial Monitor (115200 baud).

Verify system initialization.

🚀 Usage

Power on → OLED shows “System Ready.”

Place bottle (≥ 60g empty weight).

Drink tracking → Detected via lift + replace (≥ 5mL).

Blynk app → Monitor water intake in real-time.

Reminders → Buzzer beeps every 60s + Blynk notification.

Daily reset → Intake resets at midnight.

🛠 Troubleshooting
❌ HX711 Issues

Check wiring (DOUT=32, SCK=33).

If negative readings → swap A+ / A-.

Try pins 18 & 19 if 32 & 33 fail.

Run calibration sketch (see below).

❌ WiFi/Blynk Issues

Verify SSID, password, auth token.

Check WiFi signal strength.

❌ RTC Issues

Ensure I2C wiring (SDA=21, SCL=22).

Replace RTC battery if time incorrect.

⚖️ HX711 Calibration

Use HX711_Calibration.ino:

Upload sketch.

Open Serial Monitor.

Press 't' to tare.

Place known weight (e.g., 100g).

Adjust calibration_factor with + / - keys until accurate.

Update factor in main project.


📜 License

This project is licensed under the MIT License. See LICENSE
 for details.
