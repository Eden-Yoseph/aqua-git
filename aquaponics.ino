#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "config.h"  // Include configuration file

// Wi-Fi Credentials (now from config.h)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// NTP Configuration (now from config.h)
const char* ntpServer = NTP_SERVER;
const long  gmtOffset_sec = GMT_OFFSET_SEC;
const int   daylightOffset_sec = DAYLIGHT_OFFSET_SEC;

DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool grow = false;  // Manual override for grow light
unsigned long lastReadTime = 0;
const unsigned long readInterval = READ_INTERVAL;

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
  if(type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
    // Send current sensor data to newly connected client
    sendWebSocketData();
  } else if(type == WS_EVT_DISCONNECT) {
    Serial.println("WebSocket client disconnected");
  } else if(type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      String message = String((char *)data);
      DynamicJsonDocument doc(200);
      deserializeJson(doc, message);
      
      // Handle WebSocket commands
      if (doc["action"] == "growLight") {
        grow = (doc["state"] == "on");
        Serial.println(grow ? "Grow light turned on" : "Grow light turned off");
        digitalWrite(GROW_LIGHT_PIN, grow ? HIGH : LOW);
      }
    }
  }
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "2025-03-07T00:00:00"; // Fallback time
  }
  char timeString[25];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(timeString);
}

float analogToVoltage(int analogValue) {
  return analogValue * (3.3 / 4095.0); // Convert ADC reading to voltage for ESP32
}

float readTDSSensor(float temperature) {
  // Get median reading from multiple samples
  const int NUM_SAMPLES = SENSOR_SAMPLES;
  int samples[NUM_SAMPLES];
  for (int i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = analogRead(TDS_PIN);
    delay(10);
  }
  
  // Sort samples
  for (int i = 0; i < NUM_SAMPLES - 1; i++) {
    for (int j = i + 1; j < NUM_SAMPLES; j++) {
      if (samples[i] > samples[j]) {
        int temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }
  
  // Get median value
  int medianValue = samples[NUM_SAMPLES / 2];
  
  // Convert to voltage
  float voltage = analogToVoltage(medianValue);
  
  // Temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
  float compensationCoefficient = 1.0 + 0.02 * (temperature - TDS_TEMP_COMPENSATION);
  float compensationVoltage = voltage / compensationCoefficient;
  
  // Convert voltage to TDS value
  float tdsValue = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 
                  255.86 * compensationVoltage * compensationVoltage + 
                  857.39 * compensationVoltage) * TDS_K_VALUE;
  
  return tdsValue;
}

float readPHSensor() {
  // Get average reading from multiple samples
  const int NUM_SAMPLES = SENSOR_SAMPLES;
  float voltage_sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    voltage_sum += analogToVoltage(analogRead(PH_SENSOR_PIN));
    delay(10);
  }
  float voltage = voltage_sum / NUM_SAMPLES;
  
  // Convert voltage to pH value using linear interpolation
  float ph = 7.0 + ((PH_7_VOLTAGE - voltage) / PH_SLOPE) + PH_OFFSET;
  
  // Constrain pH value to valid range
  if (ph < 0) ph = 0;
  if (ph > 14) ph = 14;
  
  return ph;
}

void readDHTSensor(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    temperature = humidity = -1; // Error values
  }
}

float readWaterLevel() {
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000); // Timeout to prevent hanging
  if (duration == 0) {
    Serial.println("Ultrasonic sensor timeout");
    return -1;
  }
  float level = duration * 0.034 / 2;
  level = TANK_HEIGHT_CM - level;  // Inverting the reading (closer = more water)
  level = map(level, 0, TANK_HEIGHT_CM, 0, 100);  // Convert to percentage
  
  // Constrain to valid range
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  
  return level;
}

int readLight() {
  return analogRead(LIGHT_SENSOR_PIN);
}

float readDissolvedOxygen() {
  // Get average reading from multiple samples
  const int NUM_SAMPLES = SENSOR_SAMPLES;
  float voltage_sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    voltage_sum += analogToVoltage(analogRead(DO_PIN));
    delay(10);
  }
  return voltage_sum / NUM_SAMPLES;
}

void takeActions(float temperature, float water_level, int light_level) {
  // Fan control based on temperature
  digitalWrite(FAN_PIN, temperature > TEMP_THRESHOLD ? HIGH : LOW);
  Serial.println(temperature > TEMP_THRESHOLD ? "Fan is on" : "Fan is off");
  
  // Pump control based on water level
  digitalWrite(PUMP_PIN, water_level > WATER_LEVEL_THRESHOLD ? HIGH : LOW);
  Serial.println(water_level > WATER_LEVEL_THRESHOLD ? "Pump is on" : "Pump is off");
  
  // Grow light control based on ambient light or manual override
  static bool growLightOn = false;
  static unsigned long lastGrowLightCheck = 0;
  
  // Manual override has priority
  if (grow) {
    digitalWrite(GROW_LIGHT_PIN, HIGH);
    growLightOn = true;
    return;
  }
  
  // Automatic light control
  if (!growLightOn && light_level < LIGHT_THRESHOLD) {
    digitalWrite(GROW_LIGHT_PIN, HIGH);
    growLightOn = true;
    lastGrowLightCheck = millis();
    Serial.println("Grow light turned on");
  } else if (growLightOn && millis() - lastGrowLightCheck >= 5000) {
    // Turn off grow light and check ambient light
    digitalWrite(GROW_LIGHT_PIN, LOW);
    delay(500); // Wait for light sensor to stabilize
    int ambientLight = readLight();
    
    if (ambientLight < LIGHT_THRESHOLD) {
      digitalWrite(GROW_LIGHT_PIN, HIGH);
      Serial.println("Grow light remains on");
    } else {
      growLightOn = false;
      Serial.println("Grow light turned off");
    }
    
    lastGrowLightCheck = millis();
  }
}

void sendWebSocketData() {
  float temperature, humidity;
  readDHTSensor(temperature, humidity);
  float water_level = readWaterLevel();
  float tds_level = readTDSSensor(temperature);
  float dissolved_oxygen = readDissolvedOxygen();
  int light_level = readLight();
  float ph_level = readPHSensor();

  DynamicJsonDocument doc(1024);
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["waterLevel"] = water_level;
  doc["tds"] = tds_level;
  doc["dissolvedOxygen"] = dissolved_oxygen;
  doc["light"] = light_level;
  doc["ph"] = ph_level;
  doc["timestamp"] = getCurrentTime();

  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

void sendData() {
  if (WiFi.status() == WL_CONNECTED) {
    // Prepare data
    float temperature, humidity;
    readDHTSensor(temperature, humidity);
    float water_level = readWaterLevel();
    float tds_level = readTDSSensor(temperature);
    float dissolved_oxygen = readDissolvedOxygen();
    int light_level = readLight();
    float ph_level = readPHSensor();
    
    // Take automatic actions based on sensor readings
    takeActions(temperature, water_level, light_level);

    // Send data to API (now using config values)
    HTTPClient http;
    http.begin(String(API_BASE_URL) + String(API_ENDPOINT));
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> jsonDoc;
    jsonDoc["timestamp"] = getCurrentTime();
    jsonDoc["temperature"] = temperature;
    jsonDoc["humidity"] = humidity;
    jsonDoc["water_level"] = water_level;
    jsonDoc["tds_level"] = tds_level;
    jsonDoc["dissolved_oxygen"] = dissolved_oxygen;
    jsonDoc["light_level"] = light_level;
    jsonDoc["ph_level"] = ph_level;

    String jsonString;
    serializeJson(jsonDoc, jsonString);
    
    Serial.println("Sending data to server:");
    Serial.println(jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode > 0) {
      Serial.println("✅ Data sent successfully!");
    } else {
      Serial.print("❌ Error sending data: ");
      Serial.println(httpResponseCode);
    }
    http.end();
    
    // Also send data to connected WebSocket clients
    sendWebSocketData();
  } else {
    Serial.println("❌ No WiFi connection. Skipping data transmission.");
    // Try to reconnect
    connectToWiFi();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Aquaponic System...");
  
  // Initialize pins
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  pinMode(GROW_LIGHT_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  
  // Set initial state for actuators
  digitalWrite(GROW_LIGHT_PIN, LOW);
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize SPIFFS for web files
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Setup WebSocket server
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Setup web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");
  
  // Start server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastReadTime >= readInterval) {
    sendData(); // This also calls takeActions()
    lastReadTime = currentTime;
  }
  
  ws.cleanupClients(); // Clean up disconnected WebSocket clients
}