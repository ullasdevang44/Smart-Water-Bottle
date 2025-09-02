# Smart Water Bottle  

The **Smart Water Bottle** is an IoT-enabled project that tracks water intake using an ESP32 microcontroller, an HX711 load cell amplifier, a DS3231 RTC, an OLED display, and a buzzer for reminders. It connects to the **Blynk** platform to monitor water consumption in real-time via a mobile app.  

---

## 📑 Table of Contents
- [Project Overview](#project-overview)  
- [Features](#features)  
- [Hardware Requirements](#hardware-requirements)  
- [Circuit Diagram](#circuit-diagram)  
- [Software Requirements](#software-requirements)  
- [Setup Instructions](#setup-instructions)  
- [Usage](#usage)  
- [Troubleshooting](#troubleshooting)  
- [HX711 Calibration](#hx711-calibration)  
- [License](#license)  

---

## 📖 Project Overview  
This project monitors the water level in a bottle using a **load cell** and **HX711 amplifier**, displays the time, date, and water intake on an **OLED screen**, and sends data to the **Blynk app**.  
A **buzzer** provides periodic reminders to drink water, and the **DS3231 RTC** ensures accurate timekeeping. The system resets daily intake at midnight and tracks drinking events by detecting bottle lifts and placements.  

---

## ✨ Features  
- Measures water weight using an HX711 load cell amplifier.  
- Displays **time, date, remaining water, daily intake, and last drink** on a 128x64 OLED.  
- Sends data to **Blynk app** for remote monitoring (Virtual Pins V0, V1, V2).  
- **Buzzer reminders** every 60 seconds.  
- **Daily intake reset** at midnight using DS3231 RTC.  
- WiFi connectivity for **NTP time sync** and Blynk integration.  

---

## 🔧 Hardware Requirements  
- ESP32 Dev Module (e.g., ESP32-WROOM-32)  
- HX711 Load Cell Amplifier with a load cell (e.g., 1kg or 5kg)  
- SSD1306 128x64 OLED Display (I2C interface)  
- DS3231 RTC Module  
- Buzzer (active or passive)  
- Power Supply: 5V/1A+ USB or battery (7.4V LiPo with regulator)  
- Jumper wires, breadboard, or custom PCB  
- Water bottle (calibrated empty weight: ~60g)  

---

## ⚡ Circuit Diagram  

**HX711**  
- DOUT → ESP32 Pin 32  
- SCK → ESP32 Pin 33  
- VCC → 3.3V / 5V  
- GND → ESP32 GND  

**Load Cell**  
- E+ (Red), E- (Black), A+ (Green), A- (White) → HX711  

**OLED (I2C)**  
- SDA → ESP32 Pin 21  
- SCL → ESP32 Pin 22  
- VCC → 3.3V  
- GND → ESP32 GND  

**DS3231 RTC (I2C)**  
- SDA → ESP32 Pin 21  
- SCL → ESP32 Pin 22  
- VCC → 3.3V / 5V  
- GND → ESP32 GND  

**Buzzer**  
- Positive → ESP32 Pin 5  
- Negative → ESP32 GND  

---

## 💻 Software Requirements  
- Arduino IDE (v2.x or later)  
- ESP32 Arduino Core (via Boards Manager)  
- Libraries (see `libraries.txt`):  
  - `U8g2` (OLED)  
  - `HX711` (load cell)  
  - `RTClib` (RTC)  
  - `BlynkSimpleEsp32` (Blynk)  
  - `WiFi`, `WiFiUdp` (ESP32 core)  
  - `NTPClient` (time sync)  

**Blynk App Setup**  
- Template ID: `TMPL3oGGrYApx`  
- Auth Token: *(your token)*  
- Datastreams:  
  - V0 → Remaining water (mL)  
  - V1 → Daily intake (mL)  
  - V2 → Last drink (mL)  
- Event: `water_reminder`  

---

## ⚙️ Setup Instructions  

### Hardware Setup  
1. Connect components as per the circuit diagram.  
2. Fix the load cell so the bottle weight is measured correctly.  
3. Ensure stable 5V/1A+ power supply.  

### Software Setup  
1. Install Arduino IDE + ESP32 core.  
2. Install required libraries.  
3. Open `src/SmartWaterBottle.ino`.  
4. Update WiFi credentials (`ssid`, `pass`).  
5. Set **calibration_factor** after HX711 calibration.  

### Upload Code  
1. Select **ESP32 Dev Module** in Arduino IDE.  
2. Upload the sketch.  
3. Open Serial Monitor @ **115200 baud** to check initialization.  

---

## ▶️ Usage  
- **Power On** → OLED shows *System Ready*.  
- **Bottle Placement** → Detects baseline (≥ 60g).  
- **Drink Tracking** → Detects lifts/replacements (≥ 5mL).  
- **Blynk App** → Monitor Remaining (V0), Intake (V1), Last Drink (V2).  
- **Reminders** → Buzzer + Blynk notification every 60s.  
- **Daily Reset** → Intake auto-resets at midnight.  

---

## 🛠 Troubleshooting  

**HX711 Issues**  
- Check wiring (Pin 32 → DOUT, Pin 33 → SCK).  
- If negative values → swap A+/A-.  
- Recalibrate with known weight.  
- If unstable → try pins 18/19 or check power.  

**WiFi/Blynk**  
- Verify `ssid`, `pass`, and `BLYNK_AUTH_TOKEN`.  
- Ensure ESP32 is in WiFi range.  

**RTC Issues**  
- Check I2C wiring (SDA=21, SCL=22).  
- Replace RTC coin cell if drifting.  

---

## ⚖️ HX711 Calibration  

Save as `src/HX711_Calibration.ino`:  

```cpp
#include <HX711.h>
#define HX711_DOUT 32
#define HX711_SCK 33

HX711 scale;
float calibration_factor = 402.43;

void setup() {
  Serial.begin(115200);
  scale.begin(HX711_DOUT, HX711_SCK);
  Serial.println("Remove all weight and press 't' to tare");
}

void loop() {
  if (scale.is_ready()) {
    Serial.print("Weight: ");
    Serial.println(scale.get_units(10), 2);
  }
  if (Serial.available()) {
    char input = Serial.read();
    if (input == '+') calibration_factor += 10;
    else if (input == '-') calibration_factor -= 10;
    else if (input == 't') scale.tare();
    scale.set_scale(calibration_factor);
    Serial.print("Calibration Factor: ");
    Serial.println(calibration_factor);
  }
  delay(1000);
}
