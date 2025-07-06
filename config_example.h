#ifndef CONFIG_H
#define CONFIG_H


// CONFIGURATION TEMPLATE


// Wi-Fi Configuration
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// API Configuration
#define API_BASE_URL "http://192.168.1.100:8000"
#define API_ENDPOINT "/api/sensors/manual"

// NTP Configuration
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 14400  // UTC+4 (UAE time) - adjust for your timezone
#define DAYLIGHT_OFFSET_SEC 0

// Sensor Pin Definitions
#define DHTPIN 19
#define DHTTYPE DHT22
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 18
#define TDS_PIN 34
#define DO_PIN 35
#define LIGHT_SENSOR_PIN 32
#define PH_SENSOR_PIN 33

// Actuator Pin Definitions
#define GROW_LIGHT_PIN 22
#define PUMP_PIN 23
#define FAN_PIN 21

// Control Thresholds
#define TEMP_THRESHOLD 33.0       // Temperature threshold for fan (°C)
#define WATER_LEVEL_THRESHOLD 75  // Water level threshold for pump (%)
#define LIGHT_THRESHOLD 1500      // Light threshold for grow lights (analog value)

// Sensor Calibration Constants
#define TDS_VREF 3.3
#define TDS_TEMP_COMPENSATION 25.0
#define TDS_K_VALUE 1.0

#define PH_OFFSET 0.0      // pH calibration offset
#define PH_SLOPE 3.5       // pH sensor slope (calibrate with buffer solutions)
#define PH_4_VOLTAGE 3.0   // Voltage at pH 4 (needs calibration)
#define PH_7_VOLTAGE 2.5   // Voltage at pH 7 (needs calibration)

// Timing Configuration
#define READ_INTERVAL 5000  // Data reading interval in milliseconds
#define SENSOR_SAMPLES 10   // Number of samples for averaging/median filtering

// Physical Constants
#define TANK_HEIGHT_CM 45  // Total tank height in centimeters

#endif