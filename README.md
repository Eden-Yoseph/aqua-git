# 🌿 Smart Aquaponics System with ESP32

A real-time, sensor-integrated automation system for aquaponics environments, built using **ESP32**, **WebSockets**, and a full suite of environmental sensors. The system monitors and controls water level, temperature, pH, TDS, dissolved oxygen, light, and humidity — automatically activating actuators such as grow lights, fans, and water pumps.

## 📡 Features

- **Real-time data acquisition** from:
  - DHT22 (temperature & humidity)
  - Ultrasonic sensor (water level %)
  - TDS sensor (Total Dissolved Solids) with temperature compensation
  - pH sensor (voltage-calibrated with multi-point sampling)
  - DO sensor (dissolved oxygen)
  - LDR (light intensity)
- **WebSocket broadcasting** of live sensor data (JSON format)
- **Automated actuator control** based on configurable thresholds
- **Manual override** for grow lights via Web interface
- **NTP time synchronization** for accurate timestamping
- **RESTful API integration** (HTTP POST to external server)
- **Local web interface** hosted via SPIFFS
- **Signal processing**: Median filtering for TDS, averaging for pH/DO
- **Error handling**: Sensor timeouts and connection recovery

## 🛠️ Hardware Requirements

- **ESP32 DevKit v1**
- **DHT22** temperature/humidity sensor
- **TDS sensor** (analog)
- **pH sensor** (analog)
- **DO (Dissolved Oxygen)** analog sensor
- **Ultrasonic sensor** (HC-SR04 or compatible)
- **Light sensor** (LDR/photoresistor)
- **Actuators**: Water pump, grow light, fan
- **Relays** (if using AC actuators)

## ⚙️ System Architecture

```text
[ Sensors ] --> [ ESP32 ] --> [ WebSocket + HTTP ]
                       ↓
                [ Actuators ]
```

## 📊 API Data Format

```json
POST /api/sensors/manual
Content-Type: application/json

{
  "timestamp": "2025-03-07T14:25:33",
  "temperature": 27.5,
  "humidity": 60,
  "water_level": 83,
  "tds_level": 580.3,
  "dissolved_oxygen": 2.9,
  "light_level": 1720,
  "ph_level": 6.8
}
```

## 🚀 Setup Instructions

1. **Install Arduino IDE** with ESP32 board support
2. **Install required libraries**:
   - `WiFi.h` (ESP32 core)
   - `AsyncTCP`
   - `ESPAsyncWebServer`
   - `ArduinoJson`
   - `DHT sensor library`
3. **Configure `config.h`** with your WiFi credentials and API endpoint
4. **Upload web files** to SPIFFS partition
5. **Wire sensors** according to pin definitions
6. **Flash firmware** to ESP32

## 📈 Automation Logic

- **Fan**: Activates when temperature > 33°C
- **Pump**: Activates when water level > 75%
- **Grow Light**: Auto-activates when ambient light < 1500 (analog value)
- **Manual Override**: Available for grow lights via web interface

## 🔧 Calibration Notes

- **pH sensor**: Requires 2-point calibration (pH 4.0 and 7.0 solutions)
- **TDS sensor**: Includes temperature compensation algorithm
- **Water level**: Calibrated for 45cm tank height (configurable)

## 🛡️ Production Considerations

- Move sensitive config to external file
- Implement OTA updates
- Add data logging/SD card storage
- Consider power management for battery operation
- Add sensor fault detection and alerts

## 📝 License

MIT License - Feel free to use for educational or commercial projects.
